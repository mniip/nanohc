#ifndef UTIL_H_
#define UTIL_H_

void panic(char const *fmt, ...);
void panic_errno(char const *fmt, ...);

#define ASSERT(X) \
	if(!(X)) panic(__FILE__ ":%d: Assertion failed: " #X, __LINE__)

#endif
