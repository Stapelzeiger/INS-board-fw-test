
extern "C" {

void *__dso_handle;

}


#include <cstdlib>
#include <sys/types.h>
#include <platform-abstraction/panic.h>

void* operator new(size_t sz)
{
return malloc(sz);
}
void* operator new[](size_t sz)
{
return malloc(sz);
}
void operator delete(void*)
{
    PANIC("delete");
}
void operator delete[](void*)
{
    PANIC("delete");
}
/*
 * stdlibc++ workaround.
 * Default implementations will throw, which causes code size explosion.
 * These definitions override the ones defined in the stdlibc+++.
 */
namespace std
{
void __throw_bad_exception() { PANIC("throw"); }
void __throw_bad_alloc() { PANIC("throw"); }
void __throw_bad_cast() { PANIC("throw"); }
void __throw_bad_typeid() { PANIC("throw"); }
void __throw_logic_error(const char*) { PANIC("throw"); }
void __throw_domain_error(const char*) { PANIC("throw"); }
void __throw_invalid_argument(const char*) { PANIC("throw"); }
void __throw_length_error(const char*) { PANIC("throw"); }
void __throw_out_of_range(const char*) { PANIC("throw"); }
void __throw_runtime_error(const char*) { PANIC("throw"); }
void __throw_range_error(const char*) { PANIC("throw"); }
void __throw_overflow_error(const char*) { PANIC("throw"); }
void __throw_underflow_error(const char*) { PANIC("throw"); }
void __throw_ios_failure(const char*) { PANIC("throw"); }
void __throw_system_error(int) { PANIC("throw"); }
void __throw_future_error(int) { PANIC("throw"); }
void __throw_bad_function_call() { PANIC("throw"); }
}
namespace __gnu_cxx
{
void __verbose_terminate_handler()
{
PANIC("terminate");
}
}
extern "C"
{
int __aeabi_atexit(void*, void(*)(void*), void*)
{
return 0;
}
__extension__ typedef int __guard __attribute__((mode (__DI__)));
void __cxa_atexit(void(*)(void *), void*, void*)
{
}
int __cxa_guard_acquire(__guard* g)
{
return !*g;
}
void __cxa_guard_release (__guard* g)
{
*g = 1;
}
void __cxa_guard_abort (__guard*)
{
}
void __cxa_pure_virtual()
{
PANIC("pure virtual");
}
}