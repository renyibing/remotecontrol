#pragma once

#ifdef _WIN32

#include <string>
#include <vector>

namespace momo::svc {

// Store the original command line for use when the service starts.
void SetServiceCommandLine(int argc, char* argv[]);

// Start the service control dispatcher and run the service entry point.
int RunService();

// Service management helpers.
bool InstallService(const std::vector<std::string>& service_args);
bool UninstallService();
bool StartMomoService();
bool StopMomoService();
bool RestartMomoService();

void LogService(const std::string& message);

}  // namespace momo::svc

#endif  // _WIN32


