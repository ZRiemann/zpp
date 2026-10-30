include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

# Check the selected headers and library together, including static linking.
function(zpp_nng_check_required_api)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES nng::nng)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
  unset(ZPP_NNG_REQUIRED_API_AVAILABLE)
  unset(ZPP_NNG_REQUIRED_API_AVAILABLE CACHE)
  check_cxx_source_compiles([=[
#include <nng/nng.h>
#if NNG_MAJOR_VERSION != 2
#error zpp_nng requires NNG 2.x
#endif
int main() {
  nng_dialer dialer = NNG_DIALER_INITIALIZER;
  nng_pipe pipe = NNG_PIPE_INITIALIZER;
  nng_sockaddr address{};
  const char *scheme = nullptr;
  return nng_dialer_bind(dialer, &address) + nng_pipe_get_scheme(pipe, &scheme);
}
]=] ZPP_NNG_REQUIRED_API_AVAILABLE)
  cmake_pop_check_state()
  if(NOT ZPP_NNG_REQUIRED_API_AVAILABLE)
    message(FATAL_ERROR "zpp_nng requires NNG 2.x headers and a linkable library with nng_dialer_bind and nng_pipe_get_scheme; use the zeta_forge NNG installation")
  endif()
endfunction()
zpp_nng_check_required_api()
