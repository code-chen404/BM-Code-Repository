#pragma once

#include <QObject>
#include "ubframework_global.h"

#define		GROUP_ENGINE			"ENGINE"
#define		GROUP_STATE_MACHINE		"STATE_MACHINE"
#define		GROUP_ALG_SERVER		"ALG_SERVER"
#define		GROUP_HW_CTRL			"HW_CTRL"

#define		GROUP_PUB_SUB_PROXY		"PUB_SUB_PROXY"
#define		GROUP_REQ_REP_PROXY		"REQ_REP_PROXY"

#define		PORT_FRONT_END			"FRONT"
#define		PORT_BACK_END			"BACK"

#define		PORT_PROXY_PUB			"PROXY_PUB"
#define		PORT_PROXY_SUB			"PROXY_SUB"
#define		PORT_PROXY_ROUTER		"PROXY_ROUTER"
#define		PORT_PROXY_DEALER		"PROXY_DEALER"
#define		PORT_REQ				"REQ"
#define		PORT_PUB				"PUB"

#define		LOCAL_ADDRESS(port)		QString("tcp://127.0.0.1:%1").arg(port)
#define		HOST_ADDRESS(port)		QString("tcp://*:%1").arg(port)


class UBFRAMEWORK_EXPORT ubPortManager
{
public:
	static int initial(const char * portfile = nullptr);
	static void dump();
	static int portValue(const char * group,const char * key);

private:

};