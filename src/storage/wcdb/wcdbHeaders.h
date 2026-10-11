#ifndef WCDB_HEADERS_H
#define WCDB_HEADERS_H

// WCDB's Windows DLL package does not export all inline/template-generated
// symbols. Use ordinary external linkage so these are instantiated locally;
// non-inline WCDB functions are still resolved through its import library.
#if defined(_WIN32) && defined(WCDB_DLL)
#pragma push_macro("WCDB_DLL")
#undef WCDB_DLL
#include <WCDB/WCDBCpp.h>
#pragma pop_macro("WCDB_DLL")
#else
#include <WCDB/WCDBCpp.h>
#endif

#endif // WCDB_HEADERS_H
