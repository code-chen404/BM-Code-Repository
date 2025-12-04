#pragma once
class ubScriptEngineInterface
{
public:
	virtual int DoString(const char * buffer) = 0;
	virtual int LoadFile(const char * file) = 0;
	virtual int DoFile(const char * file)= 0;

	virtual int Run() = 0;
private:

};