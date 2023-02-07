
#ifdef __cplusplus
extern "C" {
#endif

struct SParameterizerCache;

struct SParameterizerCache
	* parameterizerQueryCacheInit();

int
	parameterizerQueryCacheGet( struct SParameterizerCache * p_psoQueryCache, const char * p_pszStmnt, OCIEnv * p_pEnv, OCIError * p_pError, OCIStmt ** p_ppQuery );

void
	parameterizerQueryCacheFini( struct SParameterizerCache * p_psoQueryCache );

#ifdef __cplusplus
}
#endif
