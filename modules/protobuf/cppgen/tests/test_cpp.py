from cppgen.cpp import CppServiceGenerator, CppMessageGenerator
from hob.proto import Proto, Quantifier, Field, Message, Request, Event, Service, assignInternalId, iterTree

import difflib

def compareOutput(out_decl, expect_decl):
    if out_decl != expect_decl:
        for line in difflib.unified_diff(expect_decl.splitlines(True), out_decl.splitlines(True), "a", "b"):
            print line,
    assert out_decl == expect_decl

def test_requiredFields():
    message = Message("RequiredFields",
                      fields=[Field(Proto.Int32,    "a", 1)
                             ,Field(Proto.Uint32,   "b", 2)
                             ,Field(Proto.Sint32,   "c", 3)
                             ,Field(Proto.Bool,     "d", 4)
                             ,Field(Proto.String,   "e", 5)
                             ,Field(Proto.Bytes,    "f", 6)
                             ,Field(Proto.Fixed32,  "g", 7)
                             ,Field(Proto.Sfixed32, "h", 8)
                             ])
    assignInternalId(iterTree([message]))
    gen = CppMessageGenerator(message, "PyTest")
    expect_decl = """class RequiredFields
{
public:
  enum MetaInfo {
      FieldCount  = 8
  };

  RequiredFields() : _a(0), _b(0), _c(0), _d(FALSE), _g(0), _h(0) {} 

  // Checkers
  BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
  BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
  BOOL HasC() const { return has_bits_.Reference().IsSet(2); }
  BOOL HasD() const { return has_bits_.Reference().IsSet(3); }
  BOOL HasE() const { return has_bits_.Reference().IsSet(4); }
  BOOL HasF() const { return has_bits_.Reference().IsSet(5); }
  BOOL HasG() const { return has_bits_.Reference().IsSet(6); }
  BOOL HasH() const { return has_bits_.Reference().IsSet(7); }

  // Getters
  INT32              GetA() const { return _a; }
  UINT32             GetB() const { return _b; }
  INT32              GetC() const { return _c; }
  BOOL               GetD() const { return _d; }
  const OpString &   GetE() const { return _e; }
  const ByteBuffer & GetF() const { return _f; }
  UINT32             GetG() const { return _g; }
  INT32              GetH() const { return _h; }

  // Setters
  void         SetA   (INT32 v) { _a = v; }
  void         SetB   (UINT32 v) { _b = v; }
  void         SetC   (INT32 v) { _c = v; }
  void         SetD   (BOOL v) { _d = v; }
  OpString &   GetERef() { return _e; }
  OP_STATUS    SetE   (const OpString & v) { return _e.Set(v); }
  OP_STATUS    SetE   (const uni_char * v, int l = KAll) { return _e.Set(v, l); }
  OP_STATUS    SetE   (const char * v, int l = KAll) { return _e.Set(v, l); }
  ByteBuffer & GetFRef() { return _f; }
  OP_STATUS    SetF   (const ByteBuffer & v) { return OpScopeCopy(v, _f); }
  OP_STATUS    SetF   (const char * v, int l) { _f.Clear(); return _f.AppendBytes(v, l); }
  void         SetG   (UINT32 v) { _g = v; }
  void         SetH   (INT32 v) { _h = v; }

  static const OpProtobufMessage &GetMessageDescriptor();

private:
  INT32      _a;
  UINT32     _b;
  INT32      _c;
  BOOL       _d;
  OpString   _e;
  ByteBuffer _f;
  UINT32     _g;
  INT32      _h;

  OpScopeBitField<FieldCount>  has_bits_;
};
"""
    compareOutput(gen.decl, expect_decl)
    expect_impl = """/*static*/
const OpProtobufMessage &
PyTest::RequiredFields::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Int32   , 1, UNI_L("a"), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Uint32  , 2, UNI_L("b"), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Sint32  , 3, UNI_L("c"), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Bool    , 4, UNI_L("d"), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::String  , 5, UNI_L("e"), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Bytes   , 6, UNI_L("f"), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Fixed32 , 7, UNI_L("g"), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Sfixed32, 8, UNI_L("h"), OpProtobufField::Required)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(PyTest::RequiredFields, _a)
      , OP_PROTO_OFFSETOF(PyTest::RequiredFields, _b)
      , OP_PROTO_OFFSETOF(PyTest::RequiredFields, _c)
      , OP_PROTO_OFFSETOF(PyTest::RequiredFields, _d)
      , OP_PROTO_OFFSETOF(PyTest::RequiredFields, _e)
      , OP_PROTO_OFFSETOF(PyTest::RequiredFields, _f)
      , OP_PROTO_OFFSETOF(PyTest::RequiredFields, _g)
      , OP_PROTO_OFFSETOF(PyTest::RequiredFields, _h)
    };
    
    static
    OpProtobufMessage message_(1, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(PyTest::RequiredFields, has_bits_), "RequiredFields", OpProtobufMessageVector<PyTest::RequiredFields>::Make, OpProtobufMessageVector<PyTest::RequiredFields>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        
    }
    return message_;
}
"""
    compareOutput(gen.impl, expect_impl)

def test_optionalFields():
    message = Message("OptionalFields",
                      fields=[Field(Proto.Int32,    "a", 1, q=Quantifier.Optional)
                             ,Field(Proto.Uint32,   "b", 2, q=Quantifier.Optional)
                             ,Field(Proto.Sint32,   "c", 3, q=Quantifier.Optional)
                             ,Field(Proto.Bool,     "d", 4, q=Quantifier.Optional)
                             ,Field(Proto.String,   "e", 5, q=Quantifier.Optional)
                             ,Field(Proto.Bytes,    "f", 6, q=Quantifier.Optional)
                             ,Field(Proto.Fixed32,  "g", 7, q=Quantifier.Optional)
                             ,Field(Proto.Sfixed32, "h", 8, q=Quantifier.Optional)
                             ])
    assignInternalId(iterTree([message]))
    gen = CppMessageGenerator(message, "PyTest")
    expect_decl = """class OptionalFields
{
public:
  enum MetaInfo {
      FieldCount  = 8
  };

  OptionalFields() : _a(0), _b(0), _c(0), _d(FALSE), _g(0), _h(0) {} 

  // Checkers
  BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
  BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
  BOOL HasC() const { return has_bits_.Reference().IsSet(2); }
  BOOL HasD() const { return has_bits_.Reference().IsSet(3); }
  BOOL HasE() const { return has_bits_.Reference().IsSet(4); }
  BOOL HasF() const { return has_bits_.Reference().IsSet(5); }
  BOOL HasG() const { return has_bits_.Reference().IsSet(6); }
  BOOL HasH() const { return has_bits_.Reference().IsSet(7); }

  // Getters
  INT32              GetA() const { return _a; }
  UINT32             GetB() const { return _b; }
  INT32              GetC() const { return _c; }
  BOOL               GetD() const { return _d; }
  const OpString &   GetE() const { return _e; }
  const ByteBuffer & GetF() const { return _f; }
  UINT32             GetG() const { return _g; }
  INT32              GetH() const { return _h; }

  // Setters
  void         SetA   (INT32 v) { has_bits_.Reference().SetBit(0); _a = v; }
  void         SetB   (UINT32 v) { has_bits_.Reference().SetBit(1); _b = v; }
  void         SetC   (INT32 v) { has_bits_.Reference().SetBit(2); _c = v; }
  void         SetD   (BOOL v) { has_bits_.Reference().SetBit(3); _d = v; }
  OpString &   GetERef() { has_bits_.Reference().SetBit(4); return _e; }
  OP_STATUS    SetE   (const OpString & v) { has_bits_.Reference().SetBit(4); return _e.Set(v); }
  OP_STATUS    SetE   (const uni_char * v, int l = KAll) { has_bits_.Reference().SetBit(4); return _e.Set(v, l); }
  OP_STATUS    SetE   (const char * v, int l = KAll) { has_bits_.Reference().SetBit(4); return _e.Set(v, l); }
  ByteBuffer & GetFRef() { has_bits_.Reference().SetBit(5); return _f; }
  OP_STATUS    SetF   (const ByteBuffer & v) { has_bits_.Reference().SetBit(5); return OpScopeCopy(v, _f); }
  OP_STATUS    SetF   (const char * v, int l) { has_bits_.Reference().SetBit(5); _f.Clear(); return _f.AppendBytes(v, l); }
  void         SetG   (UINT32 v) { has_bits_.Reference().SetBit(6); _g = v; }
  void         SetH   (INT32 v) { has_bits_.Reference().SetBit(7); _h = v; }

  static const OpProtobufMessage &GetMessageDescriptor();

private:
  INT32      _a;
  UINT32     _b;
  INT32      _c;
  BOOL       _d;
  OpString   _e;
  ByteBuffer _f;
  UINT32     _g;
  INT32      _h;

  OpScopeBitField<FieldCount>  has_bits_;
};
"""
    compareOutput(gen.decl, expect_decl)
    expect_impl = """/*static*/
const OpProtobufMessage &
PyTest::OptionalFields::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Int32   , 1, UNI_L("a"), OpProtobufField::Optional)
      , OpProtobufField(OpProtobufFormat::Uint32  , 2, UNI_L("b"), OpProtobufField::Optional)
      , OpProtobufField(OpProtobufFormat::Sint32  , 3, UNI_L("c"), OpProtobufField::Optional)
      , OpProtobufField(OpProtobufFormat::Bool    , 4, UNI_L("d"), OpProtobufField::Optional)
      , OpProtobufField(OpProtobufFormat::String  , 5, UNI_L("e"), OpProtobufField::Optional)
      , OpProtobufField(OpProtobufFormat::Bytes   , 6, UNI_L("f"), OpProtobufField::Optional)
      , OpProtobufField(OpProtobufFormat::Fixed32 , 7, UNI_L("g"), OpProtobufField::Optional)
      , OpProtobufField(OpProtobufFormat::Sfixed32, 8, UNI_L("h"), OpProtobufField::Optional)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(PyTest::OptionalFields, _a)
      , OP_PROTO_OFFSETOF(PyTest::OptionalFields, _b)
      , OP_PROTO_OFFSETOF(PyTest::OptionalFields, _c)
      , OP_PROTO_OFFSETOF(PyTest::OptionalFields, _d)
      , OP_PROTO_OFFSETOF(PyTest::OptionalFields, _e)
      , OP_PROTO_OFFSETOF(PyTest::OptionalFields, _f)
      , OP_PROTO_OFFSETOF(PyTest::OptionalFields, _g)
      , OP_PROTO_OFFSETOF(PyTest::OptionalFields, _h)
    };
    
    static
    OpProtobufMessage message_(1, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(PyTest::OptionalFields, has_bits_), "OptionalFields", OpProtobufMessageVector<PyTest::OptionalFields>::Make, OpProtobufMessageVector<PyTest::OptionalFields>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        
    }
    return message_;
}
"""
    compareOutput(gen.impl, expect_impl)

def test_repeatedFields():
    message = Message("RepeatedFields",
                      fields=[Field(Proto.Int32,    "a", 1, q=Quantifier.Repeated)
                             ,Field(Proto.Uint32,   "b", 2, q=Quantifier.Repeated)
                             ,Field(Proto.Sint32,   "c", 3, q=Quantifier.Repeated)
                             ,Field(Proto.Bool,     "d", 4, q=Quantifier.Repeated)
                             ,Field(Proto.String,   "e", 5, q=Quantifier.Repeated)
                             ,Field(Proto.Bytes,    "f", 6, q=Quantifier.Repeated)
                             ,Field(Proto.Fixed32,  "g", 7, q=Quantifier.Repeated)
                             ,Field(Proto.Sfixed32, "h", 8, q=Quantifier.Repeated)
                             ])
    assignInternalId(iterTree([message]))
    gen = CppMessageGenerator(message, "PyTest")
    expect_decl = """class RepeatedFields
{
public:
  enum MetaInfo {
      FieldCount  = 8
  };

  // Checkers
  BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
  BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
  BOOL HasC() const { return has_bits_.Reference().IsSet(2); }
  BOOL HasD() const { return has_bits_.Reference().IsSet(3); }
  BOOL HasE() const { return has_bits_.Reference().IsSet(4); }
  BOOL HasF() const { return has_bits_.Reference().IsSet(5); }
  BOOL HasG() const { return has_bits_.Reference().IsSet(6); }
  BOOL HasH() const { return has_bits_.Reference().IsSet(7); }

  // Getters
  const OpValueVector<INT32> &     GetA() const { return _a; }
  const OpValueVector<UINT32> &    GetB() const { return _b; }
  const OpValueVector<INT32> &     GetC() const { return _c; }
  const OpINT32Vector &            GetD() const { return _d; }
  const OpAutoVector<OpString> &   GetE() const { return _e; }
  const OpAutoVector<ByteBuffer> & GetF() const { return _f; }
  const OpValueVector<UINT32> &    GetG() const { return _g; }
  const OpValueVector<INT32> &     GetH() const { return _h; }

  // Setters
  OpValueVector<INT32> &     GetARef  () { has_bits_.Reference().SetBit(0); return _a; }
  OP_STATUS                  AppendToA(INT32 v) { has_bits_.Reference().SetBit(0); return _a.Add(v); }
  OpValueVector<UINT32> &    GetBRef  () { has_bits_.Reference().SetBit(1); return _b; }
  OP_STATUS                  AppendToB(UINT32 v) { has_bits_.Reference().SetBit(1); return _b.Add(v); }
  OpValueVector<INT32> &     GetCRef  () { has_bits_.Reference().SetBit(2); return _c; }
  OP_STATUS                  AppendToC(INT32 v) { has_bits_.Reference().SetBit(2); return _c.Add(v); }
  OpINT32Vector &            GetDRef  () { has_bits_.Reference().SetBit(3); return _d; }
  OP_STATUS                  AppendToD(BOOL v) { has_bits_.Reference().SetBit(3); return _d.Add(v); }
  OpAutoVector<OpString> &   GetERef  () { has_bits_.Reference().SetBit(4); return _e; }
  OP_STATUS                  AppendToE(const OpString &v) { has_bits_.Reference().SetBit(4); OpString *tmp = OP_NEW(OpString, ()); if (!tmp) return OpStatus::ERR_NO_MEMORY; RETURN_IF_ERROR(tmp->Set(v.CStr(), v.Length())); return _e.Add(tmp); }
  OP_STATUS                  AppendToE(const uni_char * v, int l = KAll) { has_bits_.Reference().SetBit(4); OpString *tmp = OP_NEW(OpString, ()); if (!tmp) return OpStatus::ERR_NO_MEMORY; RETURN_IF_ERROR(tmp->Set(v, l)); return _e.Add(tmp); }
  OP_STATUS                  AppendToE(const char * v, int l = KAll) { has_bits_.Reference().SetBit(4); OpString *tmp = OP_NEW(OpString, ()); if (!tmp) return OpStatus::ERR_NO_MEMORY; RETURN_IF_ERROR(tmp->Set(v, l)); return _e.Add(tmp); }
  OpAutoVector<ByteBuffer> & GetFRef  () { has_bits_.Reference().SetBit(5); return _f; }
  OP_STATUS                  AppendToF(const ByteBuffer &v) { has_bits_.Reference().SetBit(5); ByteBuffer *tmp = OP_NEW(ByteBuffer, ()); if (!tmp) return OpStatus::ERR_NO_MEMORY; RETURN_IF_ERROR(OpScopeCopy(v, *tmp)); return _f.Add(tmp); }
  OP_STATUS                  AppendToF(const char * v, int l) { has_bits_.Reference().SetBit(5); ByteBuffer *tmp = OP_NEW(ByteBuffer, ()); if (!tmp) return OpStatus::ERR_NO_MEMORY; RETURN_IF_ERROR(tmp->AppendBytes(v, l)); return _f.Add(tmp); }
  OpValueVector<UINT32> &    GetGRef  () { has_bits_.Reference().SetBit(6); return _g; }
  OP_STATUS                  AppendToG(UINT32 v) { has_bits_.Reference().SetBit(6); return _g.Add(v); }
  OpValueVector<INT32> &     GetHRef  () { has_bits_.Reference().SetBit(7); return _h; }
  OP_STATUS                  AppendToH(INT32 v) { has_bits_.Reference().SetBit(7); return _h.Add(v); }

  static const OpProtobufMessage &GetMessageDescriptor();

private:
  OpValueVector<INT32>     _a;
  OpValueVector<UINT32>    _b;
  OpValueVector<INT32>     _c;
  OpINT32Vector            _d;
  OpAutoVector<OpString>   _e;
  OpAutoVector<ByteBuffer> _f;
  OpValueVector<UINT32>    _g;
  OpValueVector<INT32>     _h;

  OpScopeBitField<FieldCount>  has_bits_;
};
"""
    compareOutput(gen.decl, expect_decl)
    expect_impl = """/*static*/
const OpProtobufMessage &
PyTest::RepeatedFields::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Int32   , 1, UNI_L("a"), OpProtobufField::Repeated)
      , OpProtobufField(OpProtobufFormat::Uint32  , 2, UNI_L("b"), OpProtobufField::Repeated)
      , OpProtobufField(OpProtobufFormat::Sint32  , 3, UNI_L("c"), OpProtobufField::Repeated)
      , OpProtobufField(OpProtobufFormat::Bool    , 4, UNI_L("d"), OpProtobufField::Repeated)
      , OpProtobufField(OpProtobufFormat::String  , 5, UNI_L("e"), OpProtobufField::Repeated)
      , OpProtobufField(OpProtobufFormat::Bytes   , 6, UNI_L("f"), OpProtobufField::Repeated)
      , OpProtobufField(OpProtobufFormat::Fixed32 , 7, UNI_L("g"), OpProtobufField::Repeated)
      , OpProtobufField(OpProtobufFormat::Sfixed32, 8, UNI_L("h"), OpProtobufField::Repeated)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(PyTest::RepeatedFields, _a)
      , OP_PROTO_OFFSETOF(PyTest::RepeatedFields, _b)
      , OP_PROTO_OFFSETOF(PyTest::RepeatedFields, _c)
      , OP_PROTO_OFFSETOF(PyTest::RepeatedFields, _d)
      , OP_PROTO_OFFSETOF(PyTest::RepeatedFields, _e)
      , OP_PROTO_OFFSETOF(PyTest::RepeatedFields, _f)
      , OP_PROTO_OFFSETOF(PyTest::RepeatedFields, _g)
      , OP_PROTO_OFFSETOF(PyTest::RepeatedFields, _h)
    };
    
    static
    OpProtobufMessage message_(1, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(PyTest::RepeatedFields, has_bits_), "RepeatedFields", OpProtobufMessageVector<PyTest::RepeatedFields>::Make, OpProtobufMessageVector<PyTest::RepeatedFields>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        
    }
    return message_;
}
"""
    compareOutput(gen.impl, expect_impl)

def test_subMessages():
    sub_message = Message("SubMessage",
                          fields=[Field(Proto.Int32,    "a", 1, q=Quantifier.Repeated)
                                 ])
    message = Message("MainMessage",
                      fields=[Field(Proto.Message, "a", 1, q=Quantifier.Required, message=sub_message)
                             ,Field(Proto.Message, "b", 2, q=Quantifier.Optional, message=sub_message)
                             ,Field(Proto.Message, "c", 3, q=Quantifier.Repeated, message=sub_message)
                             ])
    assignInternalId(iterTree([message]))
    gen = CppMessageGenerator(message, "PyTest")
    expect_decl = """class MainMessage
{
public:
  enum MetaInfo {
      FieldCount  = 3
  };

  MainMessage() : _b(NULL) {} 
  ~MainMessage()
  {
      OP_DELETE(_b);
  }

  // Checkers
  BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
  BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
  BOOL HasC() const { return has_bits_.Reference().IsSet(2); }

  // Getters
  const SubMessage &                          GetA() const { return _a; }
  SubMessage *                                GetB() const { return _b; }
  const OpProtobufMessageVector<SubMessage> & GetC() const { return _c; }

  // Setters
  SubMessage &                          GetARef     () { return _a; }
  void                                  SetB        (SubMessage * v) { has_bits_.Reference().SetBit(1); OP_DELETE(_b); _b = v; }
  SubMessage *                          NewB_L      () { has_bits_.Reference().SetBit(1); OP_DELETE(_b); _b = OP_NEW_L(SubMessage, ()); return _b; }
  OpProtobufMessageVector<SubMessage> & GetCRef     () { has_bits_.Reference().SetBit(2); return _c; }
  SubMessage *                          AppendNewC  () { has_bits_.Reference().SetBit(2); SubMessage *tmp = OP_NEW(SubMessage, ()); if (!tmp) return NULL; RETURN_VALUE_IF_ERROR(_c.Add(tmp), NULL); return tmp; }
  SubMessage *                          AppendNewC_L() { has_bits_.Reference().SetBit(2); SubMessage *tmp = OP_NEW_L(SubMessage, ()); _c.AddL(tmp); return tmp; }
  OP_STATUS                             AppendToC   (SubMessage * v) { has_bits_.Reference().SetBit(2); if (!v) return OpStatus::ERR_NULL_POINTER; RETURN_IF_ERROR(_c.Add(v)); }

  static const OpProtobufMessage &GetMessageDescriptor();

private:
  SubMessage                          _a;
  SubMessage *                        _b;
  OpProtobufMessageVector<SubMessage> _c;

  OpScopeBitField<FieldCount>  has_bits_;
};
"""
    compareOutput(gen.decl, expect_decl)
    expect_impl = """/*static*/
const OpProtobufMessage &
PyTest::MainMessage::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Message, 1, UNI_L("a"), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Message, 2, UNI_L("b"), OpProtobufField::Optional)
      , OpProtobufField(OpProtobufFormat::Message, 3, UNI_L("c"), OpProtobufField::Repeated)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(PyTest::MainMessage, _a)
      , OP_PROTO_OFFSETOF(PyTest::MainMessage, _b)
      , OP_PROTO_OFFSETOF(PyTest::MainMessage, _c)
    };
    
    static
    OpProtobufMessage message_(1, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(PyTest::MainMessage, has_bits_), "MainMessage", OpProtobufMessageVector<PyTest::MainMessage>::Make, OpProtobufMessageVector<PyTest::MainMessage>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        fields_[0].SetMessage(&PyTest::SubMessage::GetMessageDescriptor());
        fields_[1].SetMessage(&PyTest::SubMessage::GetMessageDescriptor());
        fields_[2].SetMessage(&PyTest::SubMessage::GetMessageDescriptor());
        
    }
    return message_;
}
"""
    compareOutput(gen.impl, expect_impl)

def test_deeplyNestedMessages():
    worm = Message("Worm",
                   fields=[Field(Proto.Bool, "isHungry", 1)
                          ])
    apple = Message("Apple",
                    fields=[Field(Proto.Bool,    "isRed", 1)
                           ,Field(Proto.Message, "wormList", 2, q=Quantifier.Repeated, message=worm)
                           ])
    pie = Message("ApplePie", children=[worm, apple],
                  fields=[Field(Proto.Message, "appleList", 1, q=Quantifier.Repeated, message=apple)
                         ])
    assignInternalId(iterTree([pie]))
    gen = CppMessageGenerator(pie, "PyTest")
    expect_decl = """class ApplePie
{
public:
  class Worm
  {
  public:
    enum MetaInfo {
        FieldCount  = 1
    };
  
    Worm() : _isHungry(FALSE) {} 
  
    // Checkers
    BOOL HasIsHungry() const { return has_bits_.Reference().IsSet(0); }
  
    // Getters
    BOOL GetIsHungry() const { return _isHungry; }
  
    // Setters
    void SetIsHungry(BOOL v) { _isHungry = v; }
  
    static const OpProtobufMessage &GetMessageDescriptor();
  
  private:
    BOOL _isHungry;
  
    OpScopeBitField<FieldCount>  has_bits_;
  };

  class Apple
  {
  public:
    enum MetaInfo {
        FieldCount  = 2
    };
  
    Apple() : _isRed(FALSE) {} 
  
    // Checkers
    BOOL HasIsRed   () const { return has_bits_.Reference().IsSet(0); }
    BOOL HasWormList() const { return has_bits_.Reference().IsSet(1); }
  
    // Getters
    BOOL                                  GetIsRed   () const { return _isRed; }
    const OpProtobufMessageVector<Worm> & GetWormList() const { return _wormList; }
  
    // Setters
    void                            SetIsRed           (BOOL v) { _isRed = v; }
    OpProtobufMessageVector<Worm> & GetWormListRef     () { has_bits_.Reference().SetBit(1); return _wormList; }
    Worm *                          AppendNewWormList  () { has_bits_.Reference().SetBit(1); Worm *tmp = OP_NEW(Worm, ()); if (!tmp) return NULL; RETURN_VALUE_IF_ERROR(_wormList.Add(tmp), NULL); return tmp; }
    Worm *                          AppendNewWormList_L() { has_bits_.Reference().SetBit(1); Worm *tmp = OP_NEW_L(Worm, ()); _wormList.AddL(tmp); return tmp; }
    OP_STATUS                       AppendToWormList   (Worm * v) { has_bits_.Reference().SetBit(1); if (!v) return OpStatus::ERR_NULL_POINTER; RETURN_IF_ERROR(_wormList.Add(v)); }
  
    static const OpProtobufMessage &GetMessageDescriptor();
  
  private:
    BOOL                          _isRed;
    OpProtobufMessageVector<Worm> _wormList;
  
    OpScopeBitField<FieldCount>  has_bits_;
  };

  enum MetaInfo {
      FieldCount  = 1
  };

  // Checkers
  BOOL HasAppleList() const { return has_bits_.Reference().IsSet(0); }

  // Getters
  const OpProtobufMessageVector<Apple> & GetAppleList() const { return _appleList; }

  // Setters
  OpProtobufMessageVector<Apple> & GetAppleListRef     () { has_bits_.Reference().SetBit(0); return _appleList; }
  Apple *                          AppendNewAppleList  () { has_bits_.Reference().SetBit(0); Apple *tmp = OP_NEW(Apple, ()); if (!tmp) return NULL; RETURN_VALUE_IF_ERROR(_appleList.Add(tmp), NULL); return tmp; }
  Apple *                          AppendNewAppleList_L() { has_bits_.Reference().SetBit(0); Apple *tmp = OP_NEW_L(Apple, ()); _appleList.AddL(tmp); return tmp; }
  OP_STATUS                        AppendToAppleList   (Apple * v) { has_bits_.Reference().SetBit(0); if (!v) return OpStatus::ERR_NULL_POINTER; RETURN_IF_ERROR(_appleList.Add(v)); }

  static const OpProtobufMessage &GetMessageDescriptor();

private:
  OpProtobufMessageVector<Apple> _appleList;

  OpScopeBitField<FieldCount>  has_bits_;
};
"""
    compareOutput(gen.decl, expect_decl)
    expect_impl = """/*static*/
const OpProtobufMessage &
PyTest::ApplePie::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Message, 1, UNI_L("appleList"), OpProtobufField::Repeated)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(PyTest::ApplePie, _appleList)
    };
    
    static
    OpProtobufMessage message_(1, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(PyTest::ApplePie, has_bits_), "ApplePie", OpProtobufMessageVector<PyTest::ApplePie>::Make, OpProtobufMessageVector<PyTest::ApplePie>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        fields_[0].SetMessage(&PyTest::ApplePie::Apple::GetMessageDescriptor());
        
    }
    return message_;
}

/* BEGIN: Sub-Messages of ApplePie */

/*static*/
const OpProtobufMessage &
PyTest::ApplePie::Worm::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Bool, 1, UNI_L("isHungry"), OpProtobufField::Required)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(PyTest::ApplePie::Worm, _isHungry)
    };
    
    static
    OpProtobufMessage message_(2, 1, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(PyTest::ApplePie::Worm, has_bits_), "Worm", OpProtobufMessageVector<PyTest::ApplePie::Worm>::Make, OpProtobufMessageVector<PyTest::ApplePie::Worm>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        
    }
    return message_;
}

/*static*/
const OpProtobufMessage &
PyTest::ApplePie::Apple::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Bool   , 1, UNI_L("isRed"   ), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Message, 2, UNI_L("wormList"), OpProtobufField::Repeated)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(PyTest::ApplePie::Apple, _isRed)
      , OP_PROTO_OFFSETOF(PyTest::ApplePie::Apple, _wormList)
    };
    
    static
    OpProtobufMessage message_(3, 1, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(PyTest::ApplePie::Apple, has_bits_), "Apple", OpProtobufMessageVector<PyTest::ApplePie::Apple>::Make, OpProtobufMessageVector<PyTest::ApplePie::Apple>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        fields_[1].SetMessage(&PyTest::ApplePie::Worm::GetMessageDescriptor());
        
    }
    return message_;
}


/* END:  Sub-Messages of ApplePie */
"""
    compareOutput(gen.impl, expect_impl)

def test_emptyMessage():
    void = Message("Void",
                   fields=[])
    assignInternalId(iterTree([void]))
    gen = CppMessageGenerator(void, "PyTest")
    expect_decl = """class Void
{
public:
  enum MetaInfo {
      FieldCount  = 0
  };

  // Checkers

  // Getters

  // Setters

  static const OpProtobufMessage &GetMessageDescriptor();

private:

  OpScopeBitField<FieldCount>  has_bits_;
};
"""
    compareOutput(gen.decl, expect_decl)
    expect_impl = """/*static*/
const OpProtobufMessage &
PyTest::Void::GetMessageDescriptor()
{
    static
    OpProtobufMessage message_(1, 0, FieldCount, NULL, NULL, OP_PROTO_OFFSETOF(PyTest::Void, has_bits_), "Void", OpProtobufMessageVector<PyTest::Void>::Make, OpProtobufMessageVector<PyTest::Void>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        
    }
    return message_;
}
"""
    compareOutput(gen.impl, expect_impl)

def test_globalMessagesInService():
    property_data = Message("Property",
                            fields=[Field(Proto.Uint32, "value", 1)
                                   ])
    object_value = Message("ObjectValue",
                           fields=[Field(Proto.Uint32, "value", 1)
                               ])
    object_info = Message("ObjectInfo", children=[property_data],
                          fields=[Field(Proto.Message, "value",       1, message=object_value)
                                 ,Field(Proto.Message, "propertyList",  2, q=Quantifier.Repeated, message=property_data)
                                 ])
    
    object_chain = Message("ObjectChain", 
                          fields=[Field(Proto.Message, "objectList", 1, q=Quantifier.Repeated, message=object_info)
                                 ])
                                 
    object_chain_list = Message("ObjectChainList",
                          fields=[Field(Proto.Message, "objectChainList", 1, q=Quantifier.Repeated, message=object_chain)
                                 ])

    assignInternalId(iterTree([object_chain_list]))

    service = Service("TestService",
                      commands=[Request(1,  "ExamineChain",        False,   object_chain_list)],
                      options={"cpp_class": "TestService",
                               "cpp_hfile": "test_service.h",
                               "version": "1.0",
                               "coreRelease": "1.0",
                               })
    gen = CppServiceGenerator(service)
#    gen = CppMessageGenerator(object_chain_list, "PyTest")
    expect_decl = """
#ifdef OP_SCOPE_STP

class TestService_SI
{
public:
    enum CommandEnum
    {
        Command_ExamineChain = 1

      , Command_Count = 1 // Number of commands in this service
    };

    // BEGIN: Message classes

    // Forward declarations
    class Default;
    class ObjectInfo;
    class ObjectChain;
    class ObjectChainList;
    class ObjectValue;

    class ObjectInfo
    {
    public:
      class Property
      {
      public:
        enum MetaInfo {
            FieldCount  = 1
        };
      
        Property() : _value(0) {} 
      
        // Checkers
        BOOL HasValue() const { return has_bits_.Reference().IsSet(0); }
      
        // Getters
        UINT32 GetValue() const { return _value; }
      
        // Setters
        void SetValue(UINT32 v) { _value = v; }
      
        static const OpProtobufMessage &GetMessageDescriptor();
      
      private:
        UINT32 _value;
      
        OpScopeBitField<FieldCount>  has_bits_;
      };
    
      enum MetaInfo {
          FieldCount  = 2
      };
    
      // Checkers
      BOOL HasValue       () const { return has_bits_.Reference().IsSet(0); }
      BOOL HasPropertyList() const { return has_bits_.Reference().IsSet(1); }
    
      // Getters
      const ObjectValue &                       GetValue       () const { return _value; }
      const OpProtobufMessageVector<Property> & GetPropertyList() const { return _propertyList; }
    
      // Setters
      ObjectValue &                       GetValueRef            () { return _value; }
      OpProtobufMessageVector<Property> & GetPropertyListRef     () { has_bits_.Reference().SetBit(1); return _propertyList; }
      Property *                          AppendNewPropertyList  () { has_bits_.Reference().SetBit(1); Property *tmp = OP_NEW(Property, ()); if (!tmp) return NULL; RETURN_VALUE_IF_ERROR(_propertyList.Add(tmp), NULL); return tmp; }
      Property *                          AppendNewPropertyList_L() { has_bits_.Reference().SetBit(1); Property *tmp = OP_NEW_L(Property, ()); _propertyList.AddL(tmp); return tmp; }
      OP_STATUS                           AppendToPropertyList   (Property * v) { has_bits_.Reference().SetBit(1); if (!v) return OpStatus::ERR_NULL_POINTER; RETURN_IF_ERROR(_propertyList.Add(v)); }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      ObjectValue                       _value;
      OpProtobufMessageVector<Property> _propertyList;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };

    class ObjectChain
    {
    public:
      enum MetaInfo {
          FieldCount  = 1
      };
    
      // Checkers
      BOOL HasObjectList() const { return has_bits_.Reference().IsSet(0); }
    
      // Getters
      const OpProtobufMessageVector<ObjectInfo> & GetObjectList() const { return _objectList; }
    
      // Setters
      OpProtobufMessageVector<ObjectInfo> & GetObjectListRef     () { has_bits_.Reference().SetBit(0); return _objectList; }
      ObjectInfo *                          AppendNewObjectList  () { has_bits_.Reference().SetBit(0); ObjectInfo *tmp = OP_NEW(ObjectInfo, ()); if (!tmp) return NULL; RETURN_VALUE_IF_ERROR(_objectList.Add(tmp), NULL); return tmp; }
      ObjectInfo *                          AppendNewObjectList_L() { has_bits_.Reference().SetBit(0); ObjectInfo *tmp = OP_NEW_L(ObjectInfo, ()); _objectList.AddL(tmp); return tmp; }
      OP_STATUS                             AppendToObjectList   (ObjectInfo * v) { has_bits_.Reference().SetBit(0); if (!v) return OpStatus::ERR_NULL_POINTER; RETURN_IF_ERROR(_objectList.Add(v)); }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      OpProtobufMessageVector<ObjectInfo> _objectList;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };

    class ObjectChainList
    {
    public:
      enum MetaInfo {
          FieldCount  = 1
      };
    
      // Checkers
      BOOL HasObjectChainList() const { return has_bits_.Reference().IsSet(0); }
    
      // Getters
      const OpProtobufMessageVector<ObjectChain> & GetObjectChainList() const { return _objectChainList; }
    
      // Setters
      OpProtobufMessageVector<ObjectChain> & GetObjectChainListRef     () { has_bits_.Reference().SetBit(0); return _objectChainList; }
      ObjectChain *                          AppendNewObjectChainList  () { has_bits_.Reference().SetBit(0); ObjectChain *tmp = OP_NEW(ObjectChain, ()); if (!tmp) return NULL; RETURN_VALUE_IF_ERROR(_objectChainList.Add(tmp), NULL); return tmp; }
      ObjectChain *                          AppendNewObjectChainList_L() { has_bits_.Reference().SetBit(0); ObjectChain *tmp = OP_NEW_L(ObjectChain, ()); _objectChainList.AddL(tmp); return tmp; }
      OP_STATUS                              AppendToObjectChainList   (ObjectChain * v) { has_bits_.Reference().SetBit(0); if (!v) return OpStatus::ERR_NULL_POINTER; RETURN_IF_ERROR(_objectChainList.Add(v)); }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      OpProtobufMessageVector<ObjectChain> _objectChainList;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };

    class ObjectValue
    {
    public:
      enum MetaInfo {
          FieldCount  = 1
      };
    
      ObjectValue() : _value(0) {} 
    
      // Checkers
      BOOL HasValue() const { return has_bits_.Reference().IsSet(0); }
    
      // Getters
      UINT32 GetValue() const { return _value; }
    
      // Setters
      void SetValue(UINT32 v) { _value = v; }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      UINT32 _value;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };


    // END: Message classes
};

#endif // OP_SCOPE_STP
"""
    compareOutput(gen.interface, expect_decl)
    expect_impl = """
#ifdef OP_SCOPE_STP

#include "modules/scope/src/scope_utils.h"
#include "modules/scope/src/scope_tp_message.h"
#include "modules/scope/src/scope_default_message.h"

// BEGIN: Messages

//   BEGIN: Message: ObjectInfo
/*static*/
const OpProtobufMessage &
TestService_SI::ObjectInfo::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Message, 1, UNI_L("value"       ), OpProtobufField::Required)
      , OpProtobufField(OpProtobufFormat::Message, 2, UNI_L("propertyList"), OpProtobufField::Repeated)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(TestService_SI::ObjectInfo, _value)
      , OP_PROTO_OFFSETOF(TestService_SI::ObjectInfo, _propertyList)
    };
    
    static
    OpProtobufMessage message_(3, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(TestService_SI::ObjectInfo, has_bits_), "ObjectInfo", OpProtobufMessageVector<TestService_SI::ObjectInfo>::Make, OpProtobufMessageVector<TestService_SI::ObjectInfo>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        fields_[0].SetMessage(&TestService_SI::ObjectValue::GetMessageDescriptor());
        fields_[1].SetMessage(&TestService_SI::ObjectInfo::Property::GetMessageDescriptor());
        
    }
    return message_;
}

/* BEGIN: Sub-Messages of ObjectInfo */

/*static*/
const OpProtobufMessage &
TestService_SI::ObjectInfo::Property::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Uint32, 1, UNI_L("value"), OpProtobufField::Required)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(TestService_SI::ObjectInfo::Property, _value)
    };
    
    static
    OpProtobufMessage message_(5, 3, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(TestService_SI::ObjectInfo::Property, has_bits_), "Property", OpProtobufMessageVector<TestService_SI::ObjectInfo::Property>::Make, OpProtobufMessageVector<TestService_SI::ObjectInfo::Property>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        
    }
    return message_;
}


/* END:  Sub-Messages of ObjectInfo */

//   END: Message: ObjectInfo

//   BEGIN: Message: ObjectChain
/*static*/
const OpProtobufMessage &
TestService_SI::ObjectChain::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Message, 1, UNI_L("objectList"), OpProtobufField::Repeated)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(TestService_SI::ObjectChain, _objectList)
    };
    
    static
    OpProtobufMessage message_(2, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(TestService_SI::ObjectChain, has_bits_), "ObjectChain", OpProtobufMessageVector<TestService_SI::ObjectChain>::Make, OpProtobufMessageVector<TestService_SI::ObjectChain>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        fields_[0].SetMessage(&TestService_SI::ObjectInfo::GetMessageDescriptor());
        
    }
    return message_;
}

//   END: Message: ObjectChain

//   BEGIN: Message: ObjectChainList
/*static*/
const OpProtobufMessage &
TestService_SI::ObjectChainList::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Message, 1, UNI_L("objectChainList"), OpProtobufField::Repeated)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(TestService_SI::ObjectChainList, _objectChainList)
    };
    
    static
    OpProtobufMessage message_(1, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(TestService_SI::ObjectChainList, has_bits_), "ObjectChainList", OpProtobufMessageVector<TestService_SI::ObjectChainList>::Make, OpProtobufMessageVector<TestService_SI::ObjectChainList>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        fields_[0].SetMessage(&TestService_SI::ObjectChain::GetMessageDescriptor());
        
    }
    return message_;
}

//   END: Message: ObjectChainList

//   BEGIN: Message: ObjectValue
/*static*/
const OpProtobufMessage &
TestService_SI::ObjectValue::GetMessageDescriptor()
{
    static
    OpProtobufField fields_[FieldCount] = {
        OpProtobufField(OpProtobufFormat::Uint32, 1, UNI_L("value"), OpProtobufField::Required)
    };
    
    static
    const int offsets_[FieldCount] = {
        OP_PROTO_OFFSETOF(TestService_SI::ObjectValue, _value)
    };
    
    static
    OpProtobufMessage message_(4, 0, FieldCount, fields_, offsets_, OP_PROTO_OFFSETOF(TestService_SI::ObjectValue, has_bits_), "ObjectValue", OpProtobufMessageVector<TestService_SI::ObjectValue>::Make, OpProtobufMessageVector<TestService_SI::ObjectValue>::Destroy);
    if (!message_.IsInitialized()) // Avoids multiple initialization of same object in case of recursion
    {
        message_.SetIsInitialized(TRUE);
        
    }
    return message_;
}

//   END: Message: ObjectValue


// END: Messages

#endif // OP_SCOPE_STP
"""
#    open("a.txt", "w").write(gen.interface_impl)
    compareOutput(gen.interface_impl, expect_impl)


def test_messageDependency():
    sub_message1 = Message("SubMessage1",
                          fields=[Field(Proto.Int32, "a", 1)
                                 ])
    sub_message3 = Message("Abc",
                          fields=[Field(Proto.Int32,   "a", 1)
                                 ,Field(Proto.Message, "b", 2)
                                 ])
    sub_message3.fields[1].message = sub_message3

    circular1 = Message("First",
                        fields=[Field(Proto.Int32,   "a", 1)
                                ,Field(Proto.Message, "b", 2)
                                ])
    circular2 = Message("Second",
                        fields=[Field(Proto.Int32,   "a", 1)
                                ,Field(Proto.Message, "b", 2)
                                ])
    circular1.fields[1].message = circular2
    circular2.fields[1].message = circular1

    sub_message2 = Message("SubMessage2",
                          fields=[Field(Proto.Int32, "a", 1)
                                 ,Field(Proto.Message, "b", 2, message=sub_message3)
                                 ])
    message = Message("MainMessage",
                      fields=[Field(Proto.Uint32,  "a", 1)
                             ,Field(Proto.Message, "b", 2, q=Quantifier.Optional, message=sub_message1)
                             ,Field(Proto.Message, "c", 3, q=Quantifier.Optional, message=sub_message2)
                             ,Field(Proto.Message, "d", 4, q=Quantifier.Optional, message=circular1)
                             ])

    service = Service("TestService",
                      commands=[Request(1,  "ExamineChain",        False,   message)],
                      options={"cpp_class": "TestService",
                               "cpp_hfile": "test_service.h",
                               "version": "1.0",
                               "coreRelease": "1.0",
                               })

    assignInternalId(iterTree(service.messages(False)))
    gen = CppServiceGenerator(service)
    expect_decl = """
#ifdef OP_SCOPE_STP

class TestService_SI
{
public:
    enum CommandEnum
    {
        Command_ExamineChain = 1

      , Command_Count = 1 // Number of commands in this service
    };

    // BEGIN: Message classes

    // Forward declarations
    class Default;
    class SubMessage1;
    class Abc;
    class SubMessage2;
    class First;
    class MainMessage;
    class Second;

    class SubMessage1
    {
    public:
      enum MetaInfo {
          FieldCount  = 1
      };
    
      SubMessage1() : _a(0) {} 
    
      // Checkers
      BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
    
      // Getters
      INT32 GetA() const { return _a; }
    
      // Setters
      void SetA(INT32 v) { _a = v; }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      INT32 _a;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };

    class Abc
    {
    public:
      enum MetaInfo {
          FieldCount  = 2
      };
    
      Abc() : _a(0) {} 
    
      // Checkers
      BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
      BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
    
      // Getters
      INT32       GetA() const { return _a; }
      const Abc & GetB() const { return _b; }
    
      // Setters
      void  SetA   (INT32 v) { _a = v; }
      Abc & GetBRef() { return _b; }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      INT32 _a;
      Abc   _b;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };

    class SubMessage2
    {
    public:
      enum MetaInfo {
          FieldCount  = 2
      };
    
      SubMessage2() : _a(0) {} 
    
      // Checkers
      BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
      BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
    
      // Getters
      INT32       GetA() const { return _a; }
      const Abc & GetB() const { return _b; }
    
      // Setters
      void  SetA   (INT32 v) { _a = v; }
      Abc & GetBRef() { return _b; }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      INT32 _a;
      Abc   _b;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };

    class First
    {
    public:
      enum MetaInfo {
          FieldCount  = 2
      };
    
      First() : _a(0) {} 
    
      // Checkers
      BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
      BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
    
      // Getters
      INT32          GetA() const { return _a; }
      const Second & GetB() const { return _b; }
    
      // Setters
      void     SetA   (INT32 v) { _a = v; }
      Second & GetBRef() { return _b; }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      INT32  _a;
      Second _b;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };

    class MainMessage
    {
    public:
      enum MetaInfo {
          FieldCount  = 4
      };
    
      MainMessage() : _a(0), _b(NULL), _c(NULL), _d(NULL) {} 
      ~MainMessage()
      {
          OP_DELETE(_b);
          OP_DELETE(_c);
          OP_DELETE(_d);
      }
    
      // Checkers
      BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
      BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
      BOOL HasC() const { return has_bits_.Reference().IsSet(2); }
      BOOL HasD() const { return has_bits_.Reference().IsSet(3); }
    
      // Getters
      UINT32        GetA() const { return _a; }
      SubMessage1 * GetB() const { return _b; }
      SubMessage2 * GetC() const { return _c; }
      First *       GetD() const { return _d; }
    
      // Setters
      void          SetA  (UINT32 v) { _a = v; }
      void          SetB  (SubMessage1 * v) { has_bits_.Reference().SetBit(1); OP_DELETE(_b); _b = v; }
      SubMessage1 * NewB_L() { has_bits_.Reference().SetBit(1); OP_DELETE(_b); _b = OP_NEW_L(SubMessage1, ()); return _b; }
      void          SetC  (SubMessage2 * v) { has_bits_.Reference().SetBit(2); OP_DELETE(_c); _c = v; }
      SubMessage2 * NewC_L() { has_bits_.Reference().SetBit(2); OP_DELETE(_c); _c = OP_NEW_L(SubMessage2, ()); return _c; }
      void          SetD  (First * v) { has_bits_.Reference().SetBit(3); OP_DELETE(_d); _d = v; }
      First *       NewD_L() { has_bits_.Reference().SetBit(3); OP_DELETE(_d); _d = OP_NEW_L(First, ()); return _d; }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      UINT32        _a;
      SubMessage1 * _b;
      SubMessage2 * _c;
      First *       _d;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };

    class Second
    {
    public:
      enum MetaInfo {
          FieldCount  = 2
      };
    
      Second() : _a(0) {} 
    
      // Checkers
      BOOL HasA() const { return has_bits_.Reference().IsSet(0); }
      BOOL HasB() const { return has_bits_.Reference().IsSet(1); }
    
      // Getters
      INT32         GetA() const { return _a; }
      const First & GetB() const { return _b; }
    
      // Setters
      void    SetA   (INT32 v) { _a = v; }
      First & GetBRef() { return _b; }
    
      static const OpProtobufMessage &GetMessageDescriptor();
    
    private:
      INT32 _a;
      First _b;
    
      OpScopeBitField<FieldCount>  has_bits_;
    };


    // END: Message classes
};

#endif // OP_SCOPE_STP
"""
    compareOutput(gen.interface, expect_decl)
