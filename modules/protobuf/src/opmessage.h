#ifndef OPMESSAGE_H
#define OPMESSAGE_H

#include "modules/protobuf/src/json_tools.h"

struct OpMessageAddress
{
// 	static OpMessageAddress Deserialize(OpData& data);
// 	static OpMessageAddress FromAscii(OpData& data)
// 	{ return Deserialize(data); }

	OpMessageAddress(short m = -1, short o = -1, int h = -1)
		: cm(m), co(o), ch(h) {}

	inline BOOL IsValid(void) const
	{ return cm != -1 && co != -1 && ch != -1; }

	inline BOOL IsChannel(void) const
	{ return IsValid() && ch > 0; }

	inline BOOL IsComponent(void) const
	{ return IsValid() && ch == 0 && co > 0; }

	inline BOOL IsComponentManager(void) const
	{ return IsValid() && ch == 0 && co == 0; }

	inline BOOL operator==(const OpMessageAddress& o) const
	{ return cm == o.cm && co == o.co && ch == o.ch; }

	inline BOOL operator<(const OpMessageAddress& o) const
	{ return cm < o.cm || co < o.co || ch < o.ch; }

// 	OP_STATUS Serialize(OpData& data) const;
// 
// 	/**
// 	* Convenience method for getting an ASCII string of this address
// 	*
// 	* The string has the form "X.Y.Z"
// 	*/
// 	OpData ToAscii(void) const
// 	{ OpData d; Serialize(d); return d; }

	short cm; ///< component manager number
	short co; ///< component number
	int ch; ///< channel number
};

class OpTypedMessage;

struct OpSerializedMessage
{
	static OpTypedMessage* Deserialize(OpSerializedMessage* message);

	OpSerializedMessage(OpData d) { data = d; }

	OpData data;
};

class OpTypedMessage
{
public:
	// Bring in the generated enum defining all Op*Message classes in the system
#	include "modules/protobuf/src/generated/g_opmessage_enum.part.h"

	virtual MessageType GetType() const { return m_type; }

	virtual const OpMessageAddress& GetSrc() const { return m_src; }
	virtual const OpMessageAddress& GetDst() const { return m_dst; }

	virtual void SetSrc(const OpMessageAddress& src) { m_src = src; }
	virtual void SetDst(const OpMessageAddress& dst) { m_dst = dst; }

	static OpSerializedMessage* Serialize(OpTypedMessage* message);

	virtual void Destroy(void) { OP_DELETE(this); }

	virtual OP_STATUS Serialize(OpData& data) const = 0;

protected:
	OpTypedMessage(
		MessageType type,
		const OpMessageAddress& src,
		const OpMessageAddress& dst,
		double delay)
		: m_type(type)
		, m_src(src)
		, m_dst(dst)
		, m_due_time(delay)
	{}

private:
	MessageType m_type;
	OpMessageAddress m_src;
	OpMessageAddress m_dst;
	double m_due_time;
};

#endif // OPMESSAGE_H
