#ifndef ENGINE_SERVER_SQL_STRING_HELPERS_H
#define ENGINE_SERVER_SQL_STRING_HELPERS_H

#include <base/system.h>

namespace sqlstr
{

void FuzzyString(char *pString, int size);

// anti SQL injection
void ClearString(char *pString, int size = 32);

void AgoTimeToString(int agoTime, char *pAgoString);

template<unsigned int size>
class CSqlString
{
public:
	CSqlString() {}

	CSqlString(const char *pStr)
	{
		str_copy(m_aString, pStr, size);
		str_copy(m_aClearString, pStr, size);
		ClearString(m_aClearString, sizeof(m_aClearString));
	}

	const char* Str() const { return m_aString; }
	const char* ClrStr() const { return m_aClearString; }

	CSqlString& operator=(const char *pStr)
	{
		str_copy(m_aString, pStr, size);
		str_copy(m_aClearString, pStr, size);
		ClearString(m_aClearString, sizeof(m_aClearString));
		return *this;
	}

	bool operator<(const CSqlString& other) const
	{
		return str_comp(m_aString, other.m_aString) < 0;
	}

private:
	char m_aString[size];
	char m_aClearString[size * 2 - 1];
};

}

#endif
