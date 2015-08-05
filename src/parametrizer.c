#include <oci.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "parametrizer.h"

#ifdef WIN32
#	define snprintf _snprintf
#endif

int is_sep(char c)
{
	int iRetVal = 0;
	size_t s;
	static char mcSeparator[] = { ' ', '\t', '\r', '\n', ')', '(', ',' };

	for (s = 0; s < sizeof(mcSeparator); ++s) {
		if (c == mcSeparator[s]) {
			iRetVal = 1;
			break;
		}
	}

	return iRetVal;
}

int add_value(struct SValue **p_ppsoVAlueList, struct SValue **p_ppsoLast, void *p_pvValue, ub4 p_uiType, size_t p_stLen)
{
	int iRetVal = 0;
	unsigned long long ullValue;
	struct SValue *psoTmp = NULL;
	char *pszEnd;

	psoTmp = malloc(sizeof(struct SValue));
	memset(psoTmp, 0, sizeof(struct SValue));

	psoTmp->m_uiType = p_uiType;

	switch (p_uiType) {
	case SQLT_INT:
		ullValue = strtoll(p_pvValue, &pszEnd, 10);
		if (p_pvValue == pszEnd) {
			free(psoTmp);
			psoTmp = NULL;
			return EINVAL;
		}
		psoTmp->m_Value.m_ullVAlue = ullValue;
		break;
	case SQLT_STR:
		psoTmp->m_Value.m_pszValue = malloc(p_stLen - 1);
		strncpy(psoTmp->m_Value.m_pszValue, (char*)p_pvValue + 1, p_stLen-2);
		psoTmp->m_Value.m_pszValue[p_stLen - 2] = '\0';
		break;
	default:
		free(psoTmp);
		psoTmp = NULL;
		iRetVal = EINVAL;
		return iRetVal;
	}

	if (NULL == *p_ppsoLast) {
		*p_ppsoVAlueList = psoTmp;
		*p_ppsoLast = *p_ppsoVAlueList;
		psoTmp->m_uiPos = 1;
		snprintf((char*)psoTmp->m_mcParamName, sizeof(psoTmp->m_mcParamName) - 1, ":p%u", psoTmp->m_uiPos);
	} else {
		psoTmp->m_uiPos = (*p_ppsoLast)->m_uiPos + 1;
		snprintf((char*)psoTmp->m_mcParamName, sizeof(psoTmp->m_mcParamName) - 1, ":p%u", psoTmp->m_uiPos);
		(*p_ppsoLast)->m_psoNext = psoTmp;
		*p_ppsoLast = psoTmp;
	}

	return iRetVal;
}

int select_term(char *p_pszText, char **p_ppszTerm, size_t *p_stLen)
{
	int iRetVal = 0;
	char *pszBegin;
	size_t st = 0;
	int iCnt = 0;

	pszBegin = strstr(p_pszText, "=>");
	if (!pszBegin)
		return iRetVal;
	pszBegin++;
	pszBegin++;
	iRetVal = 1;

	for (*p_ppszTerm = pszBegin; pszBegin[st]; st++) {
		if (pszBegin[st] == '(') {
			iCnt++;
		} else if (pszBegin[st] == ')') {
			if (!iCnt) {
				break;
			}
			iCnt--;
		} else if (!iCnt && (pszBegin[st] == ',' || pszBegin[st] == ')')) {
			break;
		}
	}

	*p_stLen = st;

	return iRetVal;
}

int select_term_apo(char *p_pszText, char **p_ppszTerm, size_t *p_stLen)
{
	int iRetVal = 0;
	char *pszBegin;
	size_t st = 1;
	int iItWasApo = 0;

	pszBegin = strstr(p_pszText, "'");
	if (!pszBegin)
		return iRetVal;
	iRetVal = 1;

	for (*p_ppszTerm = pszBegin; pszBegin[st]; st++) {
		if (iItWasApo) {
			/* если после кавычки следует кавычка, значит это экранированная кавычка */
			if (pszBegin[st] == '\'') {
				iItWasApo = 0;
			} else {
				/* иначе это конец строки */
				break;
			}
		} else {
			if (pszBegin[st] == '\'')
				iItWasApo = 1;
		}
	}

	*p_stLen = st;

	return iRetVal;
}

int parse_term(struct SValue **p_ppsoVAlueList, struct SValue **p_ppsoLast, char *p_pszTerm, size_t p_stLen, char **p_ppszAlt)
{
	int iRetVal = 0;
	int iFnRes;
	int iItCouldBeString = 0;
	int iItCouldBeNumber = 0;
	int iItCouldBeRWord = 0;
	int iItWasApo = 0;
	size_t stStrLen;
	char *pszValue = NULL;
	char mcAlt[4096] = { 0 };
	size_t stReadInd = 0;
	size_t stWritInd = 0;
	size_t st;

	for (st = 0; st < p_stLen; ++st) {
		if (iItCouldBeRWord) {
			if (is_sep(p_pszTerm[st])) {
				iItCouldBeRWord = 0;
			}
			continue;
		}
		if (iItCouldBeNumber) {
			if (is_sep(p_pszTerm[st])) {
				iItCouldBeNumber = 0;
				add_value(p_ppsoVAlueList, p_ppsoLast, pszValue, SQLT_INT, stStrLen);
				iRetVal = 1;
				memcpy(&mcAlt[stWritInd], &p_pszTerm[stReadInd], pszValue - p_pszTerm - stReadInd);
				stWritInd += pszValue - p_pszTerm - stReadInd;
				iFnRes = snprintf((char*)&mcAlt[stWritInd], sizeof(mcAlt) - stWritInd - 1, "%s", (*p_ppsoLast)->m_mcParamName);
				if (0 > iFnRes)
					return ENOMEM;
				stWritInd += iFnRes;
				stReadInd = pszValue + stStrLen - p_pszTerm;
			}
			stStrLen++;
			continue;
		}
		if (iItCouldBeString) {
			stStrLen++;
			if (iItWasApo) {
				if (p_pszTerm[st] == '\'') {
					iItWasApo = 0;
					continue;
				}
				if (is_sep(p_pszTerm[st])) {
					stStrLen--;
					iItCouldBeString = 0;
				}
			}
			if (!iItCouldBeString) {
				iItCouldBeString = 0;
				add_value(p_ppsoVAlueList, p_ppsoLast, pszValue, SQLT_STR, stStrLen);
				iRetVal = 1;
				memcpy(&mcAlt[stWritInd], &p_pszTerm[stReadInd], pszValue - p_pszTerm - stReadInd);
				stWritInd += pszValue - p_pszTerm - stReadInd;
				iFnRes = snprintf((char*)&mcAlt[stWritInd], sizeof(mcAlt) - stWritInd - 1, "%s", (*p_ppsoLast)->m_mcParamName);
				if (0 > iFnRes)
					return ENOMEM;
				stWritInd += iFnRes;
				stReadInd = pszValue + stStrLen - p_pszTerm;
				continue;
			}
			if (p_pszTerm[st] == '\'')
				iItWasApo = 1;
			continue;
		}
		if (is_sep(p_pszTerm[st]))
			continue;
		if (p_pszTerm[st] == '\'') {
			iItCouldBeString = 1;
			stStrLen = 1;
			pszValue = &p_pszTerm[st];
			iItWasApo = 0;
			continue;
		}
		if (isalpha((int)p_pszTerm[st])) {
			iItCouldBeRWord = 1;
			continue;
		}
		if (isdigit((int)p_pszTerm[st])) {
			iItCouldBeNumber = 1;
			stStrLen = 1;
			pszValue = &p_pszTerm[st];
			continue;
		}
	}

	if (iItCouldBeNumber) {
		add_value(p_ppsoVAlueList, p_ppsoLast, pszValue, SQLT_INT, 0);
		iRetVal = 1;
		memcpy(&mcAlt[stWritInd], &p_pszTerm[stReadInd], pszValue - p_pszTerm - stReadInd);
		stWritInd += pszValue - p_pszTerm - stReadInd;
		iFnRes = snprintf((char*)&mcAlt[stWritInd], sizeof(mcAlt) - stWritInd - 1, "%s", (*p_ppsoLast)->m_mcParamName);
		if (0 > iFnRes)
			return ENOMEM;
		stWritInd += iFnRes;
		stReadInd = pszValue + stStrLen - p_pszTerm;
	}
	if (iItCouldBeString) {
		add_value(p_ppsoVAlueList, p_ppsoLast, pszValue, SQLT_STR, stStrLen);
		iRetVal = 1;
		memcpy(&mcAlt[stWritInd], &p_pszTerm[stReadInd], pszValue - p_pszTerm - stReadInd);
		stWritInd += pszValue - p_pszTerm - stReadInd;
		iFnRes = snprintf((char*)&mcAlt[stWritInd], sizeof(mcAlt) - stWritInd - 1, "%s", (*p_ppsoLast)->m_mcParamName);
		if (0 > iFnRes)
			return ENOMEM;
		stWritInd += iFnRes;
		stReadInd = pszValue + stStrLen - p_pszTerm;
	}

	if (stReadInd < p_stLen)
		strncat(&mcAlt[stWritInd], &p_pszTerm[stReadInd], p_stLen - stReadInd);

	*p_ppszAlt = strdup(mcAlt);

	return iRetVal;
}

void operate_query(char *p_pszQuery, struct SValue **p_ppsoValueList, char **p_pszPQuery)
{
	char mcPReq[4096] = { 0 };
	char *pszTmp;
	char *pszTerm;
	char *pszAlt;
	size_t stLen;
	size_t stWriteInd = 0;
	struct SValue *psoLast = NULL;

	pszTmp = p_pszQuery;

	if (NULL == p_pszQuery)
		return;

	/* обрабатываем все значения */
	while (select_term(pszTmp, &pszTerm, &stLen)
			|| select_term_apo(pszTmp, &pszTerm, &stLen)) {
		pszAlt = NULL;
		if (parse_term(p_ppsoValueList, &psoLast, pszTerm, stLen, &pszAlt)) {
			memcpy(&mcPReq[stWriteInd], pszTmp, pszTerm - pszTmp);
			stWriteInd += pszTerm - pszTmp;
			if (pszAlt) {
				strcat(&mcPReq[stWriteInd], pszAlt);
				stWriteInd += strlen(pszAlt);
			}
		} else {
			memcpy(&mcPReq[stWriteInd], pszTmp, pszTerm - pszTmp);
			stWriteInd += pszTerm - pszTmp;
			memcpy(&mcPReq[stWriteInd], pszTerm, stLen);
			stWriteInd += stLen;
		}
		pszTmp = pszTerm + stLen;
		if (pszAlt)
			free (pszAlt);
	}

	if (pszTmp)
		strcat(&mcPReq[stWriteInd], pszTmp);

	if (*p_ppsoValueList)
		*p_pszPQuery = strdup(mcPReq);
}

int bind_variables(struct SValue *p_psoValueList, OCIStmt *p_psoStmt, OCIError *p_psoError)
{
	int iRetVal = OCI_SUCCESS;
	struct SValue *psoTmp = NULL;

	for (psoTmp = p_psoValueList; psoTmp; psoTmp = psoTmp->m_psoNext) {
		switch (psoTmp->m_uiType) {
		case SQLT_STR:
#ifndef WIN32
			iRetVal = OCIBindByName (p_psoStmt, &psoTmp->m_hBind, p_psoError,
				psoTmp->m_mcParamName, strlen(psoTmp->m_mcParamName),
				psoTmp->m_Value.m_pszValue, (sword)strlen(psoTmp->m_Value.m_pszValue) + 1, psoTmp->m_uiType,
				(dvoid*)0, (ub2*)0, (ub2*)0, (ub4)0, (ub4*)0, OCI_DEFAULT);
#endif
			break;
		case SQLT_INT:
#ifndef WIN32
			iRetVal = OCIBindByName (p_psoStmt, &psoTmp->m_hBind, p_psoError,
				psoTmp->m_mcParamName, strlen(psoTmp->m_mcParamName),
				&psoTmp->m_Value.m_ullVAlue, (sword)sizeof(psoTmp->m_Value.m_ullVAlue), psoTmp->m_uiType,
				(dvoid*)0, (ub2*)0, (ub2*)0, (ub4)0, (ub4*)0, OCI_DEFAULT);
#endif
			break;
		default:
			iRetVal = -1;
			break;
		}
		if (iRetVal)
			break;
	}

	return iRetVal;
}

void parametrizer_cleanup(char **p_ppszQuery, struct SValue **p_ppsoValueList)
{
	struct SValue *psoTmp = NULL;

	if (*p_ppszQuery) {
		free(*p_ppszQuery);
		*p_ppszQuery = NULL;
	}
	while (*p_ppsoValueList) {
		psoTmp = *p_ppsoValueList;
		*p_ppsoValueList = psoTmp->m_psoNext;
		if (psoTmp->m_uiType == SQLT_STR)
			free(psoTmp->m_Value.m_pszValue);
		free(psoTmp);
	}
}
