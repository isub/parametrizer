struct SValue {
	ub4 m_uiType;
	char m_uiPos;
	char m_mcParamName[32];
	union {
		unsigned long long m_ullVAlue;
		char *m_pszValue;
	} m_Value;
	struct SValue *m_psoNext;
};

void operate_query(char *p_pszQuery, struct SValue **p_ppsoVAlueList, char **p_pszPQuery);