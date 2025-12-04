#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariant>
#include "ubframework_global.h"

#define ENUM_TO_STR(t)	#t

#define		ENGINE_IDN		"__ubEngine__"

//property macros
#define		UB_BASE(p)			__base_##p
#define		UB_RT(p)			__runtime_##p
#define		UB_ADV(p)			__advance_##p
#define		UB_ELEMENT(p)		__element_##p
#define		UB_LINK(p)			__link_##p
#define		UB_ROUTE(p)			__route_##p


#define		UB_STR(p)			#p

namespace ubBase {
	//Q_NAMESPACE
	
	/**
	 * Sequencer事件
	 */
	enum SEQ_EVENT
	{
		EVENT_SEQ_START=0,
		EVENT_SEQ_END,
		EVENT_NODE_START,
		EVENT_NODE_FINISHED,
		EVENT_NODE_ERROR,
		EVENT_NODE_CANEL,
	};
	//Q_ENUM_NS(SEQ_EVENT);

	/**
	 * 节点(Node)状态
	 */

	enum NODE_STATE
	{
		NODE_ERROR=-1,
		NODE_IDLE = 0,
		NODE_RUNNING,
		NODE_PASS,
		NODE_FAIL,
		NODE_SKIP,
		NODE_BYPASS,
		NODE_DISABLE
	};
	//Q_ENUM_NS(NODE_STATE);

	/**
	 * 日志系统消息等级
	 */
	enum MSG_LEVEL
	{
		REPORTER = 0,
		CRITICAL,
		ERROR,
		WARNNING,
		INFOR,
		DEBUG
	};
	//Q_ENUM_NS(MSG_LEVEL);
	inline MSG_LEVEL MSG_LEVEL_ENUM(QString msg)
	{
		QVariantMap map{ {"REPORTER",REPORTER},{"CRITICAL",CRITICAL},{"ERROR",ERROR},{"WARNNING",WARNNING},{"INFOR",INFOR},{"DEBUG",DEBUG} };
		Q_ASSERT_X(map.contains(msg.toUpper()),"MSG_LEVEL_ENUM",QString("Invalid key string : %1").arg(msg).toLatin1());
		return (MSG_LEVEL)map[msg.toUpper()].toInt();
	}

	//Property Name
#define		UB_CURRENT_NODE				"__ub_current_node"
#define		UB_PARENTS_OBJECT			"__ub_parent_nodes"
#define		UB_CHILDS_OBJECT			"__ub_child_nodes"

#define		PROPERTY_NODE_GRAPHICSITEM		"__node_graphicsitem"

#define		GLOBAL_VARIABLE_NAME			"__ub_G"

//NodeType
#define		IS_NODE_CASE				"__case_node"
#define		IS_NODE_LOOP				"__loop_node"
#define		CASE_NODE_VALUE				"__case_node_value"
}