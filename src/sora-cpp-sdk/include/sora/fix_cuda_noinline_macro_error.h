/*
## Background

The following build error occurred when I updated WebRTC to M121.

```
error: use of undeclared identifier 'noinline'; did you mean 'inline'
```

This appears to be due to an updated version of libcxx included in WebRTC.

## Solution

After investigating the LLVM [PR](https://github.com/llvm/llvm-project-release-prs/pull/698) that appears to have resolved a similar issue, I found that the problem persisted despite the existence of the files added in the PR.
(libcxx did not include bits/basic_string.h, and it appeared that the files under cuda_wrappers were not included.)

Referring to the PR, I fixed the file directly and the error was resolved. Therefore, I decided to include this header file where the error occurred.
The original patch included the push_macro and pop_macro is included, but it has been omitted because there was no problem with omitting it.

*/

#undef __noinline__