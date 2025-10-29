/*
## Background

When updating WebRTC to M121, the following build error occurred:

```
error: use of undeclared identifier 'noinline'; did you mean 'inline'
```

The version of libcxx included in WebRTC was updated.

## Corresponding

I investigated the LLVM [PR](https://github.com/llvm/llvm-project-release-prs/pull/698) that seems to have solved the same problem, and found that the file added in the PR exists, but the problem continues to occur.
(libcxx does not contain bits/basic_string.h, and the files under cuda_wrappers are not included.)

Based on the above PR, the file was directly modified and the error was eliminated, so this header file is included in the location where the error occurs.
The original patch contains push_macro and pop_macro, but since it was not a problem even if they were omitted, they are omitted.

*/

#undef __noinline__
