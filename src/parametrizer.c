#include <oci.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "parametrizer.h"

#ifdef WIN32
#	define snprintf _snprintf
#endif

int is_sep(char c, char sep_list[], size_t size)
{
	int iRetVal = 0;
	size_t s;

	for (s = 0; s < size; ++s) {
		if (c == sep_list[s]) {
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

	for (*p_ppszTerm = pszBegin; pszBegin[st]; st++) {
		if (pszBegin[st] == '(') {
			iCnt++;
		} else if (pszBegin[st] == ')') {
			if (!iCnt) {
				iRetVal = 1;
				break;
			}
			iCnt--;
		} else if (!iCnt && (pszBegin[st] == ',' || pszBegin[st] == ')')) {
			iRetVal = 1;
			break;
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
	char mcSeparators[] = { ' ', '\t', '\r', '\n', ')', '(', ',' };
	char *pszValue = NULL;
	char mcAlt[4096] = { 0 };
	size_t stReadInd = 0;
	size_t stWritInd = 0;
	size_t st;

	for (st = 0; st < p_stLen; ++st) {
		if (iItCouldBeRWord) {
			if (is_sep(p_pszTerm[st], mcSeparators, sizeof(mcSeparators))) {
				iItCouldBeRWord = 0;
			}
			continue;
		}
		if (iItCouldBeNumber) {
			if (is_sep(p_pszTerm[st], mcSeparators, sizeof(mcSeparators))) {
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
				if (is_sep(p_pszTerm[st], mcSeparators, sizeof(mcSeparators))) {
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
		if (is_sep(p_pszTerm[st], mcSeparators, sizeof(mcSeparators)))
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

void replace_apo(char *p_pszStr, size_t p_stLen)
{
	char *pszBegin;
	char *pszNext;
	char *pszDApo;

	pszBegin = strstr(p_pszStr, "'");
	if (!pszBegin)
		return;
	pszBegin++;

	char *pszTmp = malloc(p_stLen);
	pszTmp[0] = '\0';

	pszNext = strstr(pszBegin, "'");
	if (!pszNext) {
		free(pszTmp);
		return;
	}
	pszDApo = strstr(pszBegin, "''");
	if (!pszDApo) {
		memcpy(pszTmp, pszBegin, pszNext - pszBegin);
		pszTmp[pszNext - pszBegin] = '\0';
	} else {
		while (pszDApo) {
			pszDApo++;
			strncat(pszTmp, pszBegin, pszDApo - pszBegin);
			pszDApo++;
			pszBegin = pszDApo;
			pszNext = strstr(pszBegin, "'");
			pszDApo = strstr(pszBegin, "''");
		}
		if (pszNext)
			strncat(pszTmp, pszBegin, pszNext - pszBegin);
	}

	if (pszTmp) {
		strcpy(p_pszStr, pszTmp);
	}

	if (pszTmp)
		free(pszTmp);
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

	/* обрабатываем все значения */
	while (select_term(pszTmp, &pszTerm, &stLen)) {
		if (parse_term(p_ppsoValueList, &psoLast, pszTerm, stLen, &pszAlt)) {
			memcpy(&mcPReq[stWriteInd], pszTmp, pszTerm - pszTmp);
			stWriteInd += pszTerm - pszTmp;
			if (pszAlt) {
				strcat(&mcPReq[stWriteInd], pszAlt);
				stWriteInd += strlen(pszAlt);
				free(pszAlt);
			}
		} else {
			memcpy(&mcPReq[stWriteInd], pszTmp, pszTerm - pszTmp);
			stWriteInd += pszTerm - pszTmp;
			memcpy(&mcPReq[stWriteInd], pszTerm, stLen);
			stWriteInd += stLen;
		}

		pszTmp = pszTerm + stLen;
	}

	if (pszTmp)
		strcat(&mcPReq[stWriteInd], pszTmp);

	if (*p_ppsoValueList)
		*p_pszPQuery = strdup(mcPReq);
}
