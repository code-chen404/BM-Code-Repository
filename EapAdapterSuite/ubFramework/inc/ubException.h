#pragma once
#include <QException>
#include "ubframework_global.h"

#define THROW_UB_EXCEPTION(error) {ubException e((error), __FILE__, __LINE__); throw e;}
#define THROW_UB_EXCEPTION_MSG(error, msg) {ubException e((error), (msg), __FILE__, __LINE__);throw e;}
#define VALIDATE_MALLOC(ptr){if((ptr)==nullptr)THROW_VIS_EXCEPTION(MEM_ERR_MALLOC);}

class UBFRAMEWORK_EXPORT ubException :public QException
{
public:
	ubException();
	ubException(const ubException&);
	ubException(int code, QString description);
	ubException(int code, const char * file, unsigned long line);
	ubException(int code, QString description, const char * file, unsigned long line);

	void setException(int code, const char * file, unsigned long line);
	void setException(int code, QString descriotion,const char * file,unsigned long line);

	void raise() const override { throw *this; }
	ubException *clone() const override { return new ubException(*this); }
	
	virtual ~ubException();

public:
	QString description() { return _exceptionDescription; }
	int code() { return _exceptionCode; }
	const QString expFile() { return _exceptionFile; }
	unsigned long expLine() { return _exceptionLine; }

	static QString exceptionDescription(int code);

private:
	unsigned long _exceptionLine{0};
	QString _exceptionFile;
	QString _exceptionDescription;
	int _exceptionCode{0};
};

