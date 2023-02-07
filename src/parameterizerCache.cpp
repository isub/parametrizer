
#include <oci.h>
#include <unordered_map>
#include <string.h>

#include "parameterizerCache.h"

struct SParameterizerCache {
	std::unordered_map< std::string, OCIStmt * > * m_pumapParamCache;
	SParameterizerCache() : m_pumapParamCache( new std::unordered_map< std::string, OCIStmt * > ) {}
	~SParameterizerCache() { delete m_pumapParamCache; }
};

static int parameterizerQueryCacheAllocateHandle( const char * p_pszStmnt, OCIEnv * p_pEnv, OCIError * p_pError, OCIStmt ** p_ppQuery )
{
	sword swRetVal;

	do {
		swRetVal = OCIHandleAlloc( reinterpret_cast< dvoid * >( p_pEnv ), reinterpret_cast< dvoid ** >( p_ppQuery ), OCI_HTYPE_STMT, 0, NULL );
		if( OCI_SUCCESS == swRetVal ) {
		} else { /* an error occurred */
			break;
		}

		swRetVal = OCIStmtPrepare( * p_ppQuery, p_pError, reinterpret_cast< const OraText * >( p_pszStmnt ), strlen( p_pszStmnt ), OCI_NTV_SYNTAX, OCI_DEFAULT );
		if( OCI_SUCCESS == swRetVal ) {
		} else { /* an error occurred */
			break;
		}
	} while( false );

	return static_cast< int > ( swRetVal );
}

SParameterizerCache
	* parameterizerQueryCacheInit()
{
	SParameterizerCache * psoRetVal;

	try {
		psoRetVal = new SParameterizerCache;
	} catch( std::bad_alloc ) {
		psoRetVal = NULL;
	}

	return psoRetVal;
}

int parameterizerQueryCacheGet( SParameterizerCache * p_psoQueryCache, const char * p_pszStmnt, OCIEnv * p_pEnv, OCIError * p_pError, OCIStmt ** p_ppQuery )
{
	int iRetVal = 0;

	const std::string strKey( p_pszStmnt );
	auto aIter = p_psoQueryCache->m_pumapParamCache->find( strKey );

	if( aIter != p_psoQueryCache->m_pumapParamCache->end() ) {
		* p_ppQuery = aIter->second;
	} else {
		iRetVal = parameterizerQueryCacheAllocateHandle( p_pszStmnt, p_pEnv, p_pError, p_ppQuery );
		if( 0 == iRetVal ) {
			p_psoQueryCache->m_pumapParamCache->insert( std::pair< std::string, OCIStmt * >( p_pszStmnt, * p_ppQuery ) );
		} else {
			iRetVal = -1;
		}
	}

	return iRetVal;
}

void parameterizerQueryCacheFini( SParameterizerCache * p_psoQueryCache )
{
	std::unordered_map< std::string, OCIStmt * >::iterator iterQueryCache = p_psoQueryCache->m_pumapParamCache->begin();

	for ( ; iterQueryCache != p_psoQueryCache->m_pumapParamCache->end(); ++ iterQueryCache ) {
		OCIHandleFree( reinterpret_cast< dvoid * >( iterQueryCache->second ), OCI_HTYPE_STMT );
	}

	delete p_psoQueryCache;
}
