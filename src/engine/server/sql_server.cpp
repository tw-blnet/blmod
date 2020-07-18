#if defined(CONF_SQL)

#include <base/system.h>
#include <engine/shared/protocol.h>
#include <engine/shared/config.h>

#include "sql_server.h"


int CSqlServer::ms_NumReadServer = 0;
int CSqlServer::ms_NumWriteServer = 0;

CSqlServer::CSqlServer(const char *pDatabase, const char *pPrefix, const char *pUser, const char *pPass, const char *pIp, int Port, lock *pGlobalLock, bool ReadOnly, bool SetUpDb) :
		m_Port(Port),
		m_SetUpDB(SetUpDb),
		m_SqlLock(),
		m_pGlobalLock(pGlobalLock)
{
	str_copy(m_aDatabase, pDatabase, sizeof(m_aDatabase));
	str_copy(m_aPrefix, pPrefix, sizeof(m_aPrefix));
	str_copy(m_aUser, pUser, sizeof(m_aUser));
	str_copy(m_aPass, pPass, sizeof(m_aPass));
	str_copy(m_aIp, pIp, sizeof(m_aIp));

	m_pDriver = 0;
	m_pConnection = 0;
	m_pResults = 0;
	m_pStatement = 0;

	ReadOnly ? ms_NumReadServer++ : ms_NumWriteServer++;
}

CSqlServer::~CSqlServer()
{
	scope_lock LockScope(&m_SqlLock);
	try
	{
		if (m_pResults)
			delete m_pResults;
		if (m_pConnection)
		{
			delete m_pConnection;
			m_pConnection = 0;
		}
		dbg_msg("sql", "SQL connection disconnected");
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "ERROR: No SQL connection: %s", e.what());
	}
	catch (const std::exception& ex)
	{
		dbg_msg("sql", "ERROR: No SQL connection: %s", ex.what());
	}
	catch (const std::string& ex)
	{
		dbg_msg("sql", "ERROR: No SQL connection: %s", ex.c_str());
	}
	catch (...)
	{
		dbg_msg("sql", "Unknown Error cause by the MySQL/C++ Connector");
	}
}

bool CSqlServer::Connect()
{
	m_SqlLock.take();

	if (m_pDriver != NULL && m_pConnection != NULL)
	{
		try
		{
			// Connect to specific database
			m_pConnection->setSchema(m_aDatabase);
			return true;
		}
		catch (sql::SQLException &e)
		{
			dbg_msg("sql", "MySQL Error: %s", e.what());
		}
		catch (const std::exception& ex)
		{
			dbg_msg("sql", "MySQL Error: %s", ex.what());
		}
		catch (const std::string& ex)
		{
			dbg_msg("sql", "MySQL Error: %s", ex.c_str());
		}
		catch (...)
		{
			dbg_msg("sql", "Unknown Error cause by the MySQL/C++ Connector");
		}

		m_SqlLock.release();
		dbg_msg("sql", "ERROR: SQL connection failed");
		return false;
	}

	try
	{
		m_pDriver = 0;
		m_pConnection = 0;
		m_pStatement = 0;

		sql::ConnectOptionsMap connection_properties;
		connection_properties["hostName"]      = sql::SQLString(m_aIp);
		connection_properties["port"]          = m_Port;
		connection_properties["userName"]      = sql::SQLString(m_aUser);
		connection_properties["password"]      = sql::SQLString(m_aPass);
		connection_properties["OPT_CONNECT_TIMEOUT"] = 10;
		connection_properties["OPT_READ_TIMEOUT"] = 10;
		connection_properties["OPT_WRITE_TIMEOUT"] = 20;
		connection_properties["OPT_RECONNECT"] = true;
		connection_properties["OPT_CHARSET_NAME"] = sql::SQLString("utf8mb4");
		connection_properties["OPT_SET_CHARSET_NAME"] = sql::SQLString("utf8mb4");

		// Create connection
		{
			scope_lock GlobalLockScope(m_pGlobalLock);
			m_pDriver = get_driver_instance();
		}
		m_pConnection = m_pDriver->connect(connection_properties);

		// Create Statement
		m_pStatement = m_pConnection->createStatement();

		// Apparently OPT_CHARSET_NAME and OPT_SET_CHARSET_NAME are not enough
		m_pStatement->execute("SET CHARACTER SET utf8mb4;");

		if (m_SetUpDB)
		{
			char aBuf[128];
			// create database
			str_format(aBuf, sizeof(aBuf), "CREATE DATABASE IF NOT EXISTS %s CHARACTER SET utf8mb4", m_aDatabase);
			m_pStatement->execute(aBuf);
		}

		// Connect to specific database
		m_pConnection->setSchema(m_aDatabase);
		dbg_msg("sql", "sql connection established");
		return true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
	}
	catch (const std::exception& ex)
	{
		dbg_msg("sql", "MySQL Error: %s", ex.what());
	}
	catch (const std::string& ex)
	{
		dbg_msg("sql", "MySQL Error: %s", ex.c_str());
	}
	catch (...)
	{
		dbg_msg("sql", "Unknown Error cause by the MySQL/C++ Connector");
	}

	dbg_msg("sql", "ERROR: sql connection failed");
	m_SqlLock.release();
	return false;
}

void CSqlServer::Disconnect()
{
	m_SqlLock.release();
}

bool CSqlServer::CreateTables()
{
	if (!Connect())
		return false;

	bool Success = false;
	try
	{
		char aBuf[1024];

		// create tables
		str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_race (Map VARCHAR(128) BINARY NOT NULL, Name VARCHAR(%d) BINARY NOT NULL, Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, Time FLOAT DEFAULT 0, Server CHAR(4), cp1 FLOAT DEFAULT 0, cp2 FLOAT DEFAULT 0, cp3 FLOAT DEFAULT 0, cp4 FLOAT DEFAULT 0, cp5 FLOAT DEFAULT 0, cp6 FLOAT DEFAULT 0, cp7 FLOAT DEFAULT 0, cp8 FLOAT DEFAULT 0, cp9 FLOAT DEFAULT 0, cp10 FLOAT DEFAULT 0, cp11 FLOAT DEFAULT 0, cp12 FLOAT DEFAULT 0, cp13 FLOAT DEFAULT 0, cp14 FLOAT DEFAULT 0, cp15 FLOAT DEFAULT 0, cp16 FLOAT DEFAULT 0, cp17 FLOAT DEFAULT 0, cp18 FLOAT DEFAULT 0, cp19 FLOAT DEFAULT 0, cp20 FLOAT DEFAULT 0, cp21 FLOAT DEFAULT 0, cp22 FLOAT DEFAULT 0, cp23 FLOAT DEFAULT 0, cp24 FLOAT DEFAULT 0, cp25 FLOAT DEFAULT 0, GameID VARCHAR(64), DDNet7 BOOL DEFAULT FALSE, KEY (Map, Name)) CHARACTER SET utf8mb4;", m_aPrefix, MAX_NAME_LENGTH);
		executeSql(aBuf);

		str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_teamrace (Map VARCHAR(128) BINARY NOT NULL, Name VARCHAR(%d) BINARY NOT NULL, Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, Time FLOAT DEFAULT 0, ID VARBINARY(16) NOT NULL, GameID VARCHAR(64), DDNet7 BOOL DEFAULT FALSE, KEY Map (Map)) CHARACTER SET utf8mb4;", m_aPrefix, MAX_NAME_LENGTH);
		executeSql(aBuf);

		str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_maps (Map VARCHAR(128) BINARY NOT NULL, Server VARCHAR(32) BINARY NOT NULL, Mapper VARCHAR(128) BINARY NOT NULL, Points INT DEFAULT 0, Stars INT DEFAULT 0, Timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, UNIQUE KEY Map (Map)) CHARACTER SET utf8mb4;", m_aPrefix);
		executeSql(aBuf);

		str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_saves (Savegame TEXT CHARACTER SET utf8mb4 BINARY NOT NULL, Map VARCHAR(128) BINARY NOT NULL, Code VARCHAR(128) BINARY NOT NULL, Timestamp TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, Server CHAR(4), DDNet7 BOOL DEFAULT FALSE, SaveID VARCHAR(36) DEFAULT NULL, UNIQUE KEY (Map, Code)) CHARACTER SET utf8mb4;", m_aPrefix);
		executeSql(aBuf);

		str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_points (Name VARCHAR(%d) BINARY NOT NULL, Points INT DEFAULT 0, UNIQUE KEY Name (Name)) CHARACTER SET utf8mb4;", m_aPrefix, MAX_NAME_LENGTH);
		executeSql(aBuf);

		str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_users ("
			"id INT AUTO_INCREMENT PRIMARY KEY,"
			"username VARCHAR(31) NOT NULL UNIQUE,"
			"password VARCHAR(63) NOT NULL,"
			"created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
			"created_ip INT UNSIGNED,"
			"last_login TIMESTAMP,"
			"last_login_ip INT UNSIGNED,"
			"rcon_lvl TINYINT NOT NULL DEFAULT 0,"
		") CHARACTER SET utf8mb4;", m_aPrefix);
		executeSql(aBuf);

		dbg_msg("sql", "Tables were created successfully");
		Success = true;
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("sql", "MySQL Error: %s", e.what());
	}

	Disconnect();
	return Success;
}

void CSqlServer::executeSql(const char *pCommand)
{
	m_pStatement->execute(pCommand);
}

void CSqlServer::executeSqlQuery(const char *pQuery)
{
	if (m_pResults)
		delete m_pResults;

	// set it to 0, so exceptions raised from executeQuery can not make m_pResults point to invalid memory
	m_pResults = 0;
	m_pResults = m_pStatement->executeQuery(pQuery);
}

#endif
