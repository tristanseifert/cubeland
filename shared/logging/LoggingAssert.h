#ifndef LOGGINGASSERT_H
#define LOGGINGASSERT_H

#define ASSERT_HALT() (std::abort())

#define ASSERT_HANDLER(x, y, z, t, ...) (Logging::assertFailed)(x, y, z, t, ##__VA_ARGS__)
#define XASSERT(x, m, ...) (!(x) && ASSERT_HANDLER(#x, __FILE__, __LINE__, m, ##__VA_ARGS__) && (ASSERT_HALT(), 1))

#endif
