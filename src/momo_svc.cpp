#include "momo_svc.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <UserEnv.h>
#include <Windows.h>
#include <WtsApi32.h>
#include <winsvc.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

extern int RunMomoApp(int argc, char* argv[]);

namespace momo::svc {

void LogService(const std::string& message) {
  std::ofstream log_file("momo_service.log", std::ios::app);
  if (!log_file.is_open()) {
    return;
  }
  auto now = std::chrono::system_clock::now();
  auto tt = std::chrono::system_clock::to_time_t(now);
  log_file << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S") << " | "
           << message << std::endl;
}

namespace {

namespace fs = std::filesystem;

constexpr bool kPreferConsoleSession = true;

std::wstring Utf8ToWide(const std::string& src);
std::wstring QuoteArg(const std::wstring& arg);

// Ensure PROC_THREAD_ATTRIBUTE_JOB_LIST is defined (MinGW compatibility)
#ifndef PROC_THREAD_ATTRIBUTE_JOB_LIST
#define PROC_THREAD_ATTRIBUTE_JOB_LIST \
  ProcThreadAttributeValue(13, FALSE, TRUE, FALSE)
#endif

// Create a Job object that:
// - Kills the child if the service dies (KILL_ON_JOB_CLOSE)
// - Allows breakaway for child-spawned processes to avoid data loss (BREAKAWAY_OK)
static HANDLE CreateChildJobObject();
static LPPROC_THREAD_ATTRIBUTE_LIST AllocateAttrList(DWORD count);

void PrintLastError(const char* message) {
  DWORD error = GetLastError();
  std::ostringstream oss;
  oss << message << " (error=" << error << ")";
  std::cerr << oss.str() << std::endl;
  LogService(oss.str());
}

std::string JoinArgs(const std::vector<std::string>& args) {
  std::ostringstream oss;
  for (size_t i = 0; i < args.size(); ++i) {
    if (i != 0) {
      oss << ' ';
    }
    oss << args[i];
  }
  return oss.str();
}

std::wstring BuildCommandLine(const std::vector<std::string>& args) {
  std::wstring result;
  for (size_t i = 0; i < args.size(); ++i) {
    auto wide = Utf8ToWide(args[i]);
    if (wide.empty()) {
      continue;
    }
    if (!result.empty()) {
      result.push_back(L' ');
    }
    result += QuoteArg(wide);
  }
  return result;
}

static HANDLE CreateChildJobObject() {
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (!job) {
    PrintLastError("CreateJobObjectW failed");
    return nullptr;
  }
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
  info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_BREAKAWAY_OK;
  if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                               sizeof(info))) {
    PrintLastError("SetInformationJobObject failed");
    CloseHandle(job);
    return nullptr;
  }
  return job;
}

static LPPROC_THREAD_ATTRIBUTE_LIST AllocateAttrList(DWORD count) {
  SIZE_T size = 0;
  InitializeProcThreadAttributeList(nullptr, count, 0, &size);
  auto list =
      (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, size);
  if (!list)
    return nullptr;
  if (!InitializeProcThreadAttributeList(list, count, 0, &size)) {
    HeapFree(GetProcessHeap(), 0, list);
    return nullptr;
  }
  return list;
}

static HANDLE DuplicateSystemTokenForSession(DWORD session_id) {
  HANDLE process_token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY,
                        &process_token)) {
    PrintLastError("OpenProcessToken failed");
    return nullptr;
  }

  HANDLE primary_token = nullptr;
  if (!DuplicateTokenEx(process_token,
                        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY |
                            TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
                        nullptr, SecurityImpersonation, TokenPrimary,
                        &primary_token)) {
    PrintLastError("DuplicateTokenEx failed");
    CloseHandle(process_token);
    return nullptr;
  }

  CloseHandle(process_token);

  if (!SetTokenInformation(primary_token, TokenSessionId, &session_id,
                           sizeof(session_id))) {
    PrintLastError("SetTokenInformation(TokenSessionId) failed");
    CloseHandle(primary_token);
    return nullptr;
  }

  return primary_token;
}

bool LaunchMomoProcess(const std::vector<std::string>& args,
                       PROCESS_INFORMATION& pi,
                       HANDLE& out_job_handle) {
  if (args.empty()) {
    LogService("LaunchMomoProcess: no arguments to launch");
    return false;
  }

  // Build the base command line and application path
  std::wstring command = BuildCommandLine(args);
  auto app = Utf8ToWide(args.front());
  std::vector<wchar_t> command_buffer(command.begin(), command.end());
  command_buffer.push_back(L'\0');
  LPWSTR command_line =
      command_buffer.empty() ? nullptr : command_buffer.data();

  // Child working directory: use current service working directory
  wchar_t cwd_buf[MAX_PATH];
  DWORD cwd_len = GetCurrentDirectoryW(MAX_PATH, cwd_buf);
  LPCWSTR child_cwd = (cwd_len > 0 && cwd_len < MAX_PATH) ? cwd_buf : nullptr;

  // Startup info (extended) and job object to manage child lifetime
  STARTUPINFOEXW six{};
  six.StartupInfo.cb = sizeof(six);
  six.StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
  six.StartupInfo.wShowWindow = SW_HIDE;
  six.lpAttributeList = AllocateAttrList(1);
  if (!six.lpAttributeList) {
    PrintLastError("AllocateProcThreadAttributeList failed");
    return false;
  }

  HANDLE job = CreateChildJobObject();
  if (!job) {
    DeleteProcThreadAttributeList(six.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, six.lpAttributeList);
    return false;
  }
  if (!UpdateProcThreadAttribute(six.lpAttributeList, 0,
                                 PROC_THREAD_ATTRIBUTE_JOB_LIST, &job,
                                 sizeof(job), nullptr, nullptr)) {
    PrintLastError(
        "UpdateProcThreadAttribute(PROC_THREAD_ATTRIBUTE_JOB_LIST) failed");
    DeleteProcThreadAttributeList(six.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, six.lpAttributeList);
    CloseHandle(job);
    return false;
  }

  ZeroMemory(&pi, sizeof(pi));

  DWORD creation_flags = EXTENDED_STARTUPINFO_PRESENT;
  bool launched = false;

  HANDLE console_token = nullptr;
  LPVOID console_env = nullptr;

  if (kPreferConsoleSession) {
    DWORD console_session = WTSGetActiveConsoleSessionId();
    if (console_session != 0xFFFFFFFF) {
      console_token = DuplicateSystemTokenForSession(console_session);
      if (console_token) {
        if (!CreateEnvironmentBlock(&console_env, console_token, FALSE)) {
          PrintLastError("CreateEnvironmentBlock failed (console session)");
          console_env = nullptr;
        }

        six.StartupInfo.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");
        BOOL created = CreateProcessAsUserW(
            console_token, app.empty() ? nullptr : app.c_str(),
            command_buffer.data(), nullptr, nullptr, FALSE,
            creation_flags | CREATE_UNICODE_ENVIRONMENT, console_env, child_cwd,
            &six.StartupInfo, &pi);
        if (created) {
          LogService(
              "LaunchMomoProcess: started via CreateProcessAsUserW in console "
              "session");
          launched = true;
        } else {
          PrintLastError(
              "CreateProcessAsUserW failed; falling back to Session 0");
        }
      }
    } else {
      LogService(
          "LaunchMomoProcess: no active console session; using Session 0");
    }
  }

  if (console_env) {
    DestroyEnvironmentBlock(console_env);
    console_env = nullptr;
  }
  if (console_token) {
    CloseHandle(console_token);
    console_token = nullptr;
  }

  if (!launched) {
    six.StartupInfo.lpDesktop = nullptr;  // service desktop
    BOOL created = CreateProcessW(app.empty() ? nullptr : app.c_str(),
                                  command_buffer.data(), nullptr, nullptr,
                                  FALSE, creation_flags | CREATE_NO_WINDOW,
                                  nullptr, child_cwd, &six.StartupInfo, &pi);
    if (!created) {
      PrintLastError("CreateProcessW failed");
      DeleteProcThreadAttributeList(six.lpAttributeList);
      HeapFree(GetProcessHeap(), 0, six.lpAttributeList);
      CloseHandle(job);
      return false;
    }
    LogService("LaunchMomoProcess: started via CreateProcessW in Session 0");
  }

  // Attribute list can be freed after CreateProcess; job remains open until child ends
  DeleteProcThreadAttributeList(six.lpAttributeList);
  HeapFree(GetProcessHeap(), 0, six.lpAttributeList);

  out_job_handle = job;

  DWORD child_sid = 0;
  if (ProcessIdToSessionId(pi.dwProcessId, &child_sid)) {
    LogService("LaunchMomoProcess: child PID " +
               std::to_string(pi.dwProcessId) +
               ", SessionId=" + std::to_string(child_sid));
  } else {
    LogService("LaunchMomoProcess: child PID " +
               std::to_string(pi.dwProcessId));
  }

  return true;
}

bool EnsureWorkingDirectory() {
  wchar_t module_path[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, module_path, MAX_PATH);
  if (len == 0 || len == MAX_PATH) {
    PrintLastError("GetModuleFileNameW failed");
    return false;
  }

  fs::path dir(std::wstring(module_path, module_path + len));
  dir = dir.parent_path();
  LogService("Executable directory: " + dir.string());

  std::error_code ec;
  for (int i = 0; i < 8 && !dir.empty(); ++i) {
    auto candidate = dir / L"config" / L"config.ini";
    if (fs::exists(candidate, ec)) {
      std::wstring wdir = dir.wstring();
      if (!SetCurrentDirectoryW(wdir.c_str())) {
        PrintLastError("SetCurrentDirectoryW failed");
        return false;
      }
      LogService("Working directory set to: " + dir.string());
      return true;
    }
    dir = dir.parent_path();
  }

  LogService("Failed to locate config/config.ini relative to executable");
  std::cerr << "Failed to locate config/config.ini relative to executable"
            << std::endl;
  return false;
}

constexpr wchar_t kServiceName[] = L"MomoService";
constexpr wchar_t kServiceDisplayName[] = L"Momo Service";
constexpr wchar_t kServiceDescription[] = L"Momo WebRTC Native Client";

HANDLE g_stop_event = nullptr;

std::vector<std::string> g_original_args;
std::mutex g_args_mutex;

SERVICE_STATUS_HANDLE g_service_status_handle = nullptr;
SERVICE_STATUS g_service_status{};

struct ServiceHandleCloser {
  void operator()(SC_HANDLE handle) const {
    if (handle != nullptr) {
      CloseServiceHandle(handle);
    }
  }
};

using ScopedServiceHandle =
    std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, ServiceHandleCloser>;

std::wstring Utf8ToWide(const std::string& src) {
  if (src.empty()) {
    return std::wstring();
  }
  int size = MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, nullptr, 0);
  if (size <= 0) {
    return std::wstring();
  }
  std::wstring dest(static_cast<size_t>(size - 1), L'\0');
  if (MultiByteToWideChar(CP_UTF8, 0, src.c_str(), -1, dest.data(), size) ==
      0) {
    return std::wstring();
  }
  return dest;
}

std::wstring QuoteArg(const std::wstring& arg) {
  if (arg.empty()) {
    return L"\"\"";
  }
  if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
    return arg;
  }

  std::wstring result;
  result.push_back(L'"');
  size_t backslashes = 0;
  for (wchar_t ch : arg) {
    if (ch == L'\\') {
      ++backslashes;
    } else if (ch == L'"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(L'"');
      backslashes = 0;
    } else {
      result.append(backslashes, L'\\');
      backslashes = 0;
      result.push_back(ch);
    }
  }
  result.append(backslashes * 2, L'\\');
  result.push_back(L'"');
  return result;
}

ScopedServiceHandle OpenServiceManager(DWORD access) {
  SC_HANDLE handle = OpenSCManagerW(nullptr, nullptr, access);
  if (!handle) {
    PrintLastError("OpenSCManagerW failed");
  }
  return ScopedServiceHandle(handle);
}

std::vector<std::string> GetSanitizedArgs() {
  std::lock_guard<std::mutex> lock(g_args_mutex);
  if (g_original_args.empty()) {
    return {"momo"};
  }

  std::vector<std::string> result;
  result.reserve(g_original_args.size());
  for (const auto& arg : g_original_args) {
    if (arg == "--service" || arg == "--installservice" ||
        arg == "--uninstallservice" || arg == "--startservice" ||
        arg == "--stopservice" || arg == "--restartservice") {
      continue;
    }
    result.push_back(arg);
  }

  if (result.empty()) {
    result.push_back(g_original_args.front());
  }

  // Ensure WebRTC logs are generated early if not explicitly configured
  auto has_opt = [](const std::vector<std::string>& a, const std::string& opt) {
    for (size_t i = 0; i + 1 < a.size(); ++i) {
      if (a[i] == opt)
        return true;
    }
    return false;
  };
  // Important: Do NOT add extra options when only the program name is present,
  // so that argc <= 1 in the child and Util::ParseArgs() loads config.ini.
  // Only inject defaults when there are already extra args.
  if (result.size() > 1 && !has_opt(result, "--log-level")) {
    result.emplace_back("--log-level");
    result.emplace_back("info");
  }
  return result;
}

void UpdateServiceStatus(DWORD state,
                         DWORD win32_exit = NO_ERROR,
                         DWORD specific_exit = 0,
                         DWORD wait_hint = 0) {
  if (!g_service_status_handle) {
    return;
  }

  g_service_status.dwCurrentState = state;
  g_service_status.dwWin32ExitCode = win32_exit;
  g_service_status.dwServiceSpecificExitCode = specific_exit;
  g_service_status.dwWaitHint = wait_hint;

  if (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING) {
    g_service_status.dwControlsAccepted = 0;
    ++g_service_status.dwCheckPoint;
  } else {
    g_service_status.dwControlsAccepted =
        SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN |
        SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE;
    g_service_status.dwCheckPoint = 0;
  }

  SetServiceStatus(g_service_status_handle, &g_service_status);
}

DWORD WINAPI HandlerEx(DWORD control,
                       DWORD event_type,
                       LPVOID event_data,
                       LPVOID /*context*/) {
  switch (control) {
    case SERVICE_CONTROL_INTERROGATE:
      if (g_service_status_handle) {
        SetServiceStatus(g_service_status_handle, &g_service_status);
      }
      return NO_ERROR;

    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_PRESHUTDOWN:
    case SERVICE_CONTROL_SHUTDOWN:
      LogService("HandlerEx: stop requested");
      UpdateServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 5000);
      if (g_stop_event) {
        SetEvent(g_stop_event);
      }
      return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE: {
      // Log session change notifications to help diagnose screen capture availability
      auto n = reinterpret_cast<WTSSESSION_NOTIFICATION*>(event_data);
      std::ostringstream oss;
      oss << "SESSIONCHANGE: event_type=" << event_type;
      if (n) {
        oss << ", sessionId=" << n->dwSessionId;
      }
      LogService(oss.str());
      return NO_ERROR;
    }

    default:
      return ERROR_CALL_NOT_IMPLEMENTED;
  }
}

void WINAPI ServiceMain(DWORD /*argc*/, LPSTR* /*argv*/) {
  LogService("ServiceMain entered");
  g_service_status_handle =
      RegisterServiceCtrlHandlerExA("MomoService", HandlerEx, nullptr);
  if (!g_service_status_handle) {
    PrintLastError("RegisterServiceCtrlHandlerExA failed");
    return;
  }

  g_service_status = {};
  g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

  UpdateServiceStatus(SERVICE_START_PENDING, NO_ERROR, 0, 5000);

  g_stop_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  if (!g_stop_event) {
    PrintLastError("CreateEvent stop_event failed");
    UpdateServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
    return;
  }

  if (!EnsureWorkingDirectory()) {
    if (g_stop_event) {
      CloseHandle(g_stop_event);
      g_stop_event = nullptr;
    }
    UpdateServiceStatus(SERVICE_STOPPED, ERROR_PATH_NOT_FOUND, 0);
    return;
  }

  auto sanitized = GetSanitizedArgs();
  LogService("Sanitized args: " + JoinArgs(sanitized));
  if (sanitized.empty()) {
    LogService("No command line arguments available; stopping service");
    if (g_stop_event) {
      CloseHandle(g_stop_event);
      g_stop_event = nullptr;
    }
    UpdateServiceStatus(SERVICE_STOPPED, ERROR_INVALID_PARAMETER, 0);
    return;
  }

  UpdateServiceStatus(SERVICE_RUNNING);

  bool service_running = true;
  while (service_running) {
    if (WaitForSingleObject(g_stop_event, 0) == WAIT_OBJECT_0) {
      LogService("Stop event set before launching process");
      break;
    }

    PROCESS_INFORMATION pi{};
    HANDLE child_job = nullptr;
    if (!LaunchMomoProcess(sanitized, pi, child_job)) {
      LogService("Failed to launch momo.exe; retrying");
      Sleep(3000);
      continue;
    }

    HANDLE wait_handles[] = {g_stop_event, pi.hProcess};
    bool restart_needed = false;

    while (true) {
      DWORD wait = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
      if (wait == WAIT_OBJECT_0) {
        LogService("Stop event signaled; terminating child process");
        TerminateProcess(pi.hProcess, ERROR_SERVICE_SPECIFIC_ERROR);
        WaitForSingleObject(pi.hProcess, 10000);
        service_running = false;
        break;
      } else if (wait == WAIT_OBJECT_0 + 1) {
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        LogService("Child process exited with code " +
                   std::to_string(exit_code));
        restart_needed = true;
        break;
      }
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (child_job) {
      CloseHandle(child_job);
      child_job =
          nullptr;  // This will kill the child if still running due to KILL_ON_JOB_CLOSE
    }

    if (!service_running) {
      break;
    }

    if (!restart_needed) {
      break;
    }

    LogService("Restarting process after delay");
    Sleep(2000);
  }

  if (g_stop_event) {
    CloseHandle(g_stop_event);
    g_stop_event = nullptr;
  }

  UpdateServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
  LogService("ServiceMain exiting");
}

bool StopServiceHandle(SC_HANDLE service) {
  SERVICE_STATUS_PROCESS status = {};
  DWORD bytes_needed = 0;
  if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                            reinterpret_cast<LPBYTE>(&status), sizeof(status),
                            &bytes_needed)) {
    PrintLastError("QueryServiceStatusEx failed");
    return false;
  }

  if (status.dwCurrentState == SERVICE_STOPPED) {
    return true;
  }

  if (!ControlService(service, SERVICE_CONTROL_STOP,
                      reinterpret_cast<LPSERVICE_STATUS>(&status))) {
    DWORD err = GetLastError();
    if (err == ERROR_SERVICE_NOT_ACTIVE) {
      return true;
    }
    PrintLastError("ControlService stop failed");
    return false;
  }

  auto start = GetTickCount64();
  while (status.dwCurrentState != SERVICE_STOPPED) {
    Sleep(std::min<DWORD>(status.dwWaitHint, 5000));
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                              reinterpret_cast<LPBYTE>(&status), sizeof(status),
                              &bytes_needed)) {
      PrintLastError("QueryServiceStatusEx failed while waiting for stop");
      return false;
    }

    if (status.dwCurrentState == SERVICE_STOPPED) {
      break;
    }

    if (GetTickCount64() - start > 30000) {
      std::cerr << "Timed out waiting for service to stop" << std::endl;
      return false;
    }
  }

  return true;
}

}  // namespace

void SetServiceCommandLine(int argc, char* argv[]) {
  std::lock_guard<std::mutex> lock(g_args_mutex);
  g_original_args.assign(argv, argv + argc);
}

int RunService() {
  static SERVICE_TABLE_ENTRYA service_table[] = {
      {const_cast<LPSTR>("MomoService"), ServiceMain}, {nullptr, nullptr}};

  if (!StartServiceCtrlDispatcherA(service_table)) {
    PrintLastError("StartServiceCtrlDispatcherA failed");
    return 1;
  }
  return 0;
}

bool InstallService(const std::vector<std::string>& service_args) {
  auto scm = OpenServiceManager(SC_MANAGER_CREATE_SERVICE);
  if (!scm) {
    return false;
  }

  wchar_t path[MAX_PATH];
  if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
    PrintLastError("GetModuleFileNameW failed");
    return false;
  }

  std::wstringstream command;
  command << QuoteArg(path) << L" --service";
  for (const auto& arg : service_args) {
    auto wide_arg = Utf8ToWide(arg);
    if (!wide_arg.empty()) {
      command << L" " << QuoteArg(wide_arg);
    }
  }

  ScopedServiceHandle service(CreateServiceW(
      scm.get(), kServiceName, kServiceDisplayName, SERVICE_ALL_ACCESS,
      SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
      command.str().c_str(), nullptr, nullptr, nullptr, nullptr, nullptr));

  if (!service) {
    DWORD err = GetLastError();
    if (err == ERROR_SERVICE_EXISTS) {
      std::cerr << "Service already exists" << std::endl;
    } else {
      PrintLastError("CreateServiceW failed");
    }
    return false;
  }

  SERVICE_DESCRIPTIONW description{};
  description.lpDescription = const_cast<LPWSTR>(kServiceDescription);
  ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DESCRIPTION,
                        &description);

  std::cout << "Service installed successfully" << std::endl;
  return true;
}

bool UninstallService() {
  auto scm = OpenServiceManager(SC_MANAGER_CONNECT);
  if (!scm) {
    return false;
  }

  ScopedServiceHandle service(OpenServiceW(
      scm.get(), kServiceName, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS));
  if (!service) {
    PrintLastError("OpenServiceW failed");
    return false;
  }

  StopServiceHandle(service.get());

  if (!DeleteService(service.get())) {
    PrintLastError("DeleteService failed");
    return false;
  }

  std::cout << "Service uninstalled" << std::endl;
  return true;
}

bool StartMomoService() {
  auto scm = OpenServiceManager(SC_MANAGER_CONNECT);
  if (!scm) {
    return false;
  }

  ScopedServiceHandle service(OpenServiceW(
      scm.get(), kServiceName, SERVICE_START | SERVICE_QUERY_STATUS));
  if (!service) {
    PrintLastError("OpenServiceW failed");
    return false;
  }

  if (!StartServiceW(service.get(), 0, nullptr)) {
    DWORD err = GetLastError();
    if (err == ERROR_SERVICE_ALREADY_RUNNING) {
      std::cout << "Service is already running" << std::endl;
      return true;
    }
    PrintLastError("StartServiceW failed");
    return false;
  }

  std::cout << "Service start initiated" << std::endl;
  return true;
}

bool StopMomoService() {
  auto scm = OpenServiceManager(SC_MANAGER_CONNECT);
  if (!scm) {
    return false;
  }

  ScopedServiceHandle service(OpenServiceW(
      scm.get(), kServiceName, SERVICE_STOP | SERVICE_QUERY_STATUS));
  if (!service) {
    PrintLastError("OpenServiceW failed");
    return false;
  }

  if (!StopServiceHandle(service.get())) {
    return false;
  }

  std::cout << "Service stopped" << std::endl;
  return true;
}

bool RestartMomoService() {
  if (!StopMomoService()) {
    return false;
  }
  return StartMomoService();
}

}  // namespace momo::svc

// end of _WIN32 guard
#endif  // _WIN32
