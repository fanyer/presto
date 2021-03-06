#ifndef _XML_EV_H_
#define _XML_EV_H_

enum XML_EV_ElementType
{
	XML_EV_UNKNOWN = 255,
	XML_EV_LISTENER = 256
};

enum XML_EV_AttrType {
    XML_EV_EVENT = 10,
	XML_EV_PHASE,
	XML_EV_TARGET,
    XML_EV_HANDLER,
	XML_EV_OBSERVER,
	XML_EV_PROPAGATE,
	XML_EV_DEFAULTACTION
};

#endif // _XML_EV_H_
