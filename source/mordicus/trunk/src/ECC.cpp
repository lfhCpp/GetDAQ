// **********************************************************************
//
// Copyright (c) 2003-2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

// Ice version 3.3.1
// Generated from file `ECC.ice'

#include <ECC.h>
#include <Ice/LocalException.h>
#include <Ice/ObjectFactory.h>
#include <Ice/BasicStream.h>
#include <IceUtil/Iterator.h>
#include <IceUtil/ScopedArray.h>

#ifndef ICE_IGNORE_VERSION
#   if ICE_INT_VERSION / 100 != 303
#       error Ice version mismatch!
#   endif
#   if ICE_INT_VERSION % 100 > 50
#       error Beta header file detected
#   endif
#   if ICE_INT_VERSION % 100 < 1
#       error Ice patch level mismatch!
#   endif
#endif

static const ::std::string __ecc__ElectronicsControlCore__test_name = "test";

static const ::std::string __ecc__ElectronicsControlCore__timeStamp_name = "timeStamp";

static const ::std::string __ecc__ElectronicsControlCore__createNode_name = "createNode";

static const ::std::string __ecc__ElectronicsControlCore__getConfig_name = "getConfig";

static const ::std::string __ecc__ElectronicsControlCore__reconfigureNode_name = "reconfigureNode";

static const ::std::string __ecc__ElectronicsControlCore__removeNode_name = "removeNode";

::Ice::Object* IceInternal::upCast(::ecc::ElectronicsControlCore* p) { return p; }
::IceProxy::Ice::Object* IceInternal::upCast(::IceProxy::ecc::ElectronicsControlCore* p) { return p; }

void
ecc::__read(::IceInternal::BasicStream* __is, ::ecc::ElectronicsControlCorePrx& v)
{
    ::Ice::ObjectPrx proxy;
    __is->read(proxy);
    if(!proxy)
    {
        v = 0;
    }
    else
    {
        v = new ::IceProxy::ecc::ElectronicsControlCore;
        v->__copyFrom(proxy);
    }
}

void
ecc::__writeNodeConfigList(::IceInternal::BasicStream* __os, const ::mdaq::hw::server::NodeConfig* begin, const ::mdaq::hw::server::NodeConfig* end)
{
    ::Ice::Int size = static_cast< ::Ice::Int>(end - begin);
    __os->writeSize(size);
    for(int i = 0; i < size; ++i)
    {
        begin[i].__write(__os);
    }
}

void
ecc::__readNodeConfigList(::IceInternal::BasicStream* __is, ::ecc::NodeConfigList& v)
{
    ::Ice::Int sz;
    __is->readSize(sz);
    __is->startSeq(sz, 4);
    v.resize(sz);
    for(int i = 0; i < sz; ++i)
    {
        v[i].__read(__is);
        __is->checkSeq();
        __is->endElement();
    }
    __is->endSeq(sz);
}

bool
ecc::SystemConfig::operator==(const SystemConfig& __rhs) const
{
    if(this == &__rhs)
    {
        return true;
    }
    if(nodes != __rhs.nodes)
    {
        return false;
    }
    return true;
}

bool
ecc::SystemConfig::operator<(const SystemConfig& __rhs) const
{
    if(this == &__rhs)
    {
        return false;
    }
    if(nodes < __rhs.nodes)
    {
        return true;
    }
    else if(__rhs.nodes < nodes)
    {
        return false;
    }
    return false;
}

void
ecc::SystemConfig::__write(::IceInternal::BasicStream* __os) const
{
    if(nodes.size() == 0)
    {
        __os->writeSize(0);
    }
    else
    {
        ::ecc::__writeNodeConfigList(__os, &nodes[0], &nodes[0] + nodes.size());
    }
}

void
ecc::SystemConfig::__read(::IceInternal::BasicStream* __is)
{
    ::ecc::__readNodeConfigList(__is, nodes);
}

ecc::NodeAlreadyExists::NodeAlreadyExists(const ::std::string& __ice_reason) :
#if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
    UserException(),
#else
    ::Ice::UserException(),
#endif
    reason(__ice_reason)
{
}

ecc::NodeAlreadyExists::~NodeAlreadyExists() throw()
{
}

static const char* __ecc__NodeAlreadyExists_name = "ecc::NodeAlreadyExists";

::std::string
ecc::NodeAlreadyExists::ice_name() const
{
    return __ecc__NodeAlreadyExists_name;
}

::Ice::Exception*
ecc::NodeAlreadyExists::ice_clone() const
{
    return new NodeAlreadyExists(*this);
}

void
ecc::NodeAlreadyExists::ice_throw() const
{
    throw *this;
}

void
ecc::NodeAlreadyExists::__write(::IceInternal::BasicStream* __os) const
{
    __os->write(::std::string("::ecc::NodeAlreadyExists"), false);
    __os->startWriteSlice();
    __os->write(reason);
    __os->endWriteSlice();
}

void
ecc::NodeAlreadyExists::__read(::IceInternal::BasicStream* __is, bool __rid)
{
    if(__rid)
    {
        ::std::string myId;
        __is->read(myId, false);
    }
    __is->startReadSlice();
    __is->read(reason);
    __is->endReadSlice();
}

void
ecc::NodeAlreadyExists::__write(const ::Ice::OutputStreamPtr&) const
{
    Ice::MarshalException ex(__FILE__, __LINE__);
    ex.reason = "exception ecc::NodeAlreadyExists was not generated with stream support";
    throw ex;
}

void
ecc::NodeAlreadyExists::__read(const ::Ice::InputStreamPtr&, bool)
{
    Ice::MarshalException ex(__FILE__, __LINE__);
    ex.reason = "exception ecc::NodeAlreadyExists was not generated with stream support";
    throw ex;
}

struct __F__ecc__NodeAlreadyExists : public ::IceInternal::UserExceptionFactory
{
    virtual void
    createAndThrow()
    {
        throw ::ecc::NodeAlreadyExists();
    }
};

static ::IceInternal::UserExceptionFactoryPtr __F__ecc__NodeAlreadyExists__Ptr = new __F__ecc__NodeAlreadyExists;

const ::IceInternal::UserExceptionFactoryPtr&
ecc::NodeAlreadyExists::ice_factory()
{
    return __F__ecc__NodeAlreadyExists__Ptr;
}

class __F__ecc__NodeAlreadyExists__Init
{
public:

    __F__ecc__NodeAlreadyExists__Init()
    {
        ::IceInternal::factoryTable->addExceptionFactory("::ecc::NodeAlreadyExists", ::ecc::NodeAlreadyExists::ice_factory());
    }

    ~__F__ecc__NodeAlreadyExists__Init()
    {
        ::IceInternal::factoryTable->removeExceptionFactory("::ecc::NodeAlreadyExists");
    }
};

static __F__ecc__NodeAlreadyExists__Init __F__ecc__NodeAlreadyExists__i;

#ifdef __APPLE__
extern "C" { void __F__ecc__NodeAlreadyExists__initializer() {} }
#endif

ecc::ReconfigFailure::ReconfigFailure(const ::std::string& __ice_reason) :
#if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
    UserException(),
#else
    ::Ice::UserException(),
#endif
    reason(__ice_reason)
{
}

ecc::ReconfigFailure::~ReconfigFailure() throw()
{
}

static const char* __ecc__ReconfigFailure_name = "ecc::ReconfigFailure";

::std::string
ecc::ReconfigFailure::ice_name() const
{
    return __ecc__ReconfigFailure_name;
}

::Ice::Exception*
ecc::ReconfigFailure::ice_clone() const
{
    return new ReconfigFailure(*this);
}

void
ecc::ReconfigFailure::ice_throw() const
{
    throw *this;
}

void
ecc::ReconfigFailure::__write(::IceInternal::BasicStream* __os) const
{
    __os->write(::std::string("::ecc::ReconfigFailure"), false);
    __os->startWriteSlice();
    __os->write(reason);
    __os->endWriteSlice();
}

void
ecc::ReconfigFailure::__read(::IceInternal::BasicStream* __is, bool __rid)
{
    if(__rid)
    {
        ::std::string myId;
        __is->read(myId, false);
    }
    __is->startReadSlice();
    __is->read(reason);
    __is->endReadSlice();
}

void
ecc::ReconfigFailure::__write(const ::Ice::OutputStreamPtr&) const
{
    Ice::MarshalException ex(__FILE__, __LINE__);
    ex.reason = "exception ecc::ReconfigFailure was not generated with stream support";
    throw ex;
}

void
ecc::ReconfigFailure::__read(const ::Ice::InputStreamPtr&, bool)
{
    Ice::MarshalException ex(__FILE__, __LINE__);
    ex.reason = "exception ecc::ReconfigFailure was not generated with stream support";
    throw ex;
}

struct __F__ecc__ReconfigFailure : public ::IceInternal::UserExceptionFactory
{
    virtual void
    createAndThrow()
    {
        throw ::ecc::ReconfigFailure();
    }
};

static ::IceInternal::UserExceptionFactoryPtr __F__ecc__ReconfigFailure__Ptr = new __F__ecc__ReconfigFailure;

const ::IceInternal::UserExceptionFactoryPtr&
ecc::ReconfigFailure::ice_factory()
{
    return __F__ecc__ReconfigFailure__Ptr;
}

class __F__ecc__ReconfigFailure__Init
{
public:

    __F__ecc__ReconfigFailure__Init()
    {
        ::IceInternal::factoryTable->addExceptionFactory("::ecc::ReconfigFailure", ::ecc::ReconfigFailure::ice_factory());
    }

    ~__F__ecc__ReconfigFailure__Init()
    {
        ::IceInternal::factoryTable->removeExceptionFactory("::ecc::ReconfigFailure");
    }
};

static __F__ecc__ReconfigFailure__Init __F__ecc__ReconfigFailure__i;

#ifdef __APPLE__
extern "C" { void __F__ecc__ReconfigFailure__initializer() {} }
#endif

void
IceProxy::ecc::ElectronicsControlCore::test(const ::std::string& testString, const ::Ice::Context* __ctx)
{
    int __cnt = 0;
    while(true)
    {
        ::IceInternal::Handle< ::IceDelegate::Ice::Object> __delBase;
        try
        {
#if defined(__BCPLUSPLUS__) && (__BCPLUSPLUS__ >= 0x0600) // C++Builder 2009 compiler bug
            IceUtil::DummyBCC dummy;
#endif
            __delBase = __getDelegate(false);
            ::IceDelegate::ecc::ElectronicsControlCore* __del = dynamic_cast< ::IceDelegate::ecc::ElectronicsControlCore*>(__delBase.get());
            __del->test(testString, __ctx);
            return;
        }
        catch(const ::IceInternal::LocalExceptionWrapper& __ex)
        {
            __handleExceptionWrapperRelaxed(__delBase, __ex, 0, __cnt);
        }
        catch(const ::Ice::LocalException& __ex)
        {
            __handleException(__delBase, __ex, 0, __cnt);
        }
    }
}

::Ice::Long
IceProxy::ecc::ElectronicsControlCore::timeStamp(const ::Ice::Context* __ctx)
{
    int __cnt = 0;
    while(true)
    {
        ::IceInternal::Handle< ::IceDelegate::Ice::Object> __delBase;
        try
        {
#if defined(__BCPLUSPLUS__) && (__BCPLUSPLUS__ >= 0x0600) // C++Builder 2009 compiler bug
            IceUtil::DummyBCC dummy;
#endif
            __checkTwowayOnly(__ecc__ElectronicsControlCore__timeStamp_name);
            __delBase = __getDelegate(false);
            ::IceDelegate::ecc::ElectronicsControlCore* __del = dynamic_cast< ::IceDelegate::ecc::ElectronicsControlCore*>(__delBase.get());
            return __del->timeStamp(__ctx);
        }
        catch(const ::IceInternal::LocalExceptionWrapper& __ex)
        {
            __handleExceptionWrapper(__delBase, __ex, 0);
        }
        catch(const ::Ice::LocalException& __ex)
        {
            __handleException(__delBase, __ex, 0, __cnt);
        }
    }
}

void
IceProxy::ecc::ElectronicsControlCore::createNode(const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Context* __ctx)
{
    int __cnt = 0;
    while(true)
    {
        ::IceInternal::Handle< ::IceDelegate::Ice::Object> __delBase;
        try
        {
#if defined(__BCPLUSPLUS__) && (__BCPLUSPLUS__ >= 0x0600) // C++Builder 2009 compiler bug
            IceUtil::DummyBCC dummy;
#endif
            __checkTwowayOnly(__ecc__ElectronicsControlCore__createNode_name);
            __delBase = __getDelegate(false);
            ::IceDelegate::ecc::ElectronicsControlCore* __del = dynamic_cast< ::IceDelegate::ecc::ElectronicsControlCore*>(__delBase.get());
            __del->createNode(config, __ctx);
            return;
        }
        catch(const ::IceInternal::LocalExceptionWrapper& __ex)
        {
            __handleExceptionWrapperRelaxed(__delBase, __ex, 0, __cnt);
        }
        catch(const ::Ice::LocalException& __ex)
        {
            __handleException(__delBase, __ex, 0, __cnt);
        }
    }
}

void
IceProxy::ecc::ElectronicsControlCore::getConfig(::ecc::NodeConfigList& nodes, const ::Ice::Context* __ctx)
{
    int __cnt = 0;
    while(true)
    {
        ::IceInternal::Handle< ::IceDelegate::Ice::Object> __delBase;
        try
        {
#if defined(__BCPLUSPLUS__) && (__BCPLUSPLUS__ >= 0x0600) // C++Builder 2009 compiler bug
            IceUtil::DummyBCC dummy;
#endif
            __checkTwowayOnly(__ecc__ElectronicsControlCore__getConfig_name);
            __delBase = __getDelegate(false);
            ::IceDelegate::ecc::ElectronicsControlCore* __del = dynamic_cast< ::IceDelegate::ecc::ElectronicsControlCore*>(__delBase.get());
            __del->getConfig(nodes, __ctx);
            return;
        }
        catch(const ::IceInternal::LocalExceptionWrapper& __ex)
        {
            __handleExceptionWrapperRelaxed(__delBase, __ex, 0, __cnt);
        }
        catch(const ::Ice::LocalException& __ex)
        {
            __handleException(__delBase, __ex, 0, __cnt);
        }
    }
}

void
IceProxy::ecc::ElectronicsControlCore::reconfigureNode(const ::mdaq::hw::server::NodeId& id, const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Context* __ctx)
{
    int __cnt = 0;
    while(true)
    {
        ::IceInternal::Handle< ::IceDelegate::Ice::Object> __delBase;
        try
        {
#if defined(__BCPLUSPLUS__) && (__BCPLUSPLUS__ >= 0x0600) // C++Builder 2009 compiler bug
            IceUtil::DummyBCC dummy;
#endif
            __checkTwowayOnly(__ecc__ElectronicsControlCore__reconfigureNode_name);
            __delBase = __getDelegate(false);
            ::IceDelegate::ecc::ElectronicsControlCore* __del = dynamic_cast< ::IceDelegate::ecc::ElectronicsControlCore*>(__delBase.get());
            __del->reconfigureNode(id, config, __ctx);
            return;
        }
        catch(const ::IceInternal::LocalExceptionWrapper& __ex)
        {
            __handleExceptionWrapperRelaxed(__delBase, __ex, 0, __cnt);
        }
        catch(const ::Ice::LocalException& __ex)
        {
            __handleException(__delBase, __ex, 0, __cnt);
        }
    }
}

void
IceProxy::ecc::ElectronicsControlCore::removeNode(const ::mdaq::hw::server::NodeId& id, const ::Ice::Context* __ctx)
{
    int __cnt = 0;
    while(true)
    {
        ::IceInternal::Handle< ::IceDelegate::Ice::Object> __delBase;
        try
        {
#if defined(__BCPLUSPLUS__) && (__BCPLUSPLUS__ >= 0x0600) // C++Builder 2009 compiler bug
            IceUtil::DummyBCC dummy;
#endif
            __delBase = __getDelegate(false);
            ::IceDelegate::ecc::ElectronicsControlCore* __del = dynamic_cast< ::IceDelegate::ecc::ElectronicsControlCore*>(__delBase.get());
            __del->removeNode(id, __ctx);
            return;
        }
        catch(const ::IceInternal::LocalExceptionWrapper& __ex)
        {
            __handleExceptionWrapperRelaxed(__delBase, __ex, 0, __cnt);
        }
        catch(const ::Ice::LocalException& __ex)
        {
            __handleException(__delBase, __ex, 0, __cnt);
        }
    }
}

const ::std::string&
IceProxy::ecc::ElectronicsControlCore::ice_staticId()
{
    return ::ecc::ElectronicsControlCore::ice_staticId();
}

::IceInternal::Handle< ::IceDelegateM::Ice::Object>
IceProxy::ecc::ElectronicsControlCore::__createDelegateM()
{
    return ::IceInternal::Handle< ::IceDelegateM::Ice::Object>(new ::IceDelegateM::ecc::ElectronicsControlCore);
}

::IceInternal::Handle< ::IceDelegateD::Ice::Object>
IceProxy::ecc::ElectronicsControlCore::__createDelegateD()
{
    return ::IceInternal::Handle< ::IceDelegateD::Ice::Object>(new ::IceDelegateD::ecc::ElectronicsControlCore);
}

::IceProxy::Ice::Object*
IceProxy::ecc::ElectronicsControlCore::__newInstance() const
{
    return new ElectronicsControlCore;
}

void
IceDelegateM::ecc::ElectronicsControlCore::test(const ::std::string& testString, const ::Ice::Context* __context)
{
    ::IceInternal::Outgoing __og(__handler.get(), __ecc__ElectronicsControlCore__test_name, ::Ice::Idempotent, __context);
    try
    {
        ::IceInternal::BasicStream* __os = __og.os();
        __os->write(testString);
    }
    catch(const ::Ice::LocalException& __ex)
    {
        __og.abort(__ex);
    }
    bool __ok = __og.invoke();
    if(!__og.is()->b.empty())
    {
        try
        {
            if(!__ok)
            {
                try
                {
                    __og.throwUserException();
                }
                catch(const ::Ice::UserException& __ex)
                {
                    ::Ice::UnknownUserException __uue(__FILE__, __LINE__, __ex.ice_name());
                    throw __uue;
                }
            }
            __og.is()->skipEmptyEncaps();
        }
        catch(const ::Ice::LocalException& __ex)
        {
            throw ::IceInternal::LocalExceptionWrapper(__ex, false);
        }
    }
}

::Ice::Long
IceDelegateM::ecc::ElectronicsControlCore::timeStamp(const ::Ice::Context* __context)
{
    ::IceInternal::Outgoing __og(__handler.get(), __ecc__ElectronicsControlCore__timeStamp_name, ::Ice::Normal, __context);
    bool __ok = __og.invoke();
    ::Ice::Long __ret;
    try
    {
        if(!__ok)
        {
            try
            {
                __og.throwUserException();
            }
            catch(const ::Ice::UserException& __ex)
            {
                ::Ice::UnknownUserException __uue(__FILE__, __LINE__, __ex.ice_name());
                throw __uue;
            }
        }
        ::IceInternal::BasicStream* __is = __og.is();
        __is->startReadEncaps();
        __is->read(__ret);
        __is->endReadEncaps();
        return __ret;
    }
    catch(const ::Ice::LocalException& __ex)
    {
        throw ::IceInternal::LocalExceptionWrapper(__ex, false);
    }
}

void
IceDelegateM::ecc::ElectronicsControlCore::createNode(const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Context* __context)
{
    ::IceInternal::Outgoing __og(__handler.get(), __ecc__ElectronicsControlCore__createNode_name, ::Ice::Idempotent, __context);
    try
    {
        ::IceInternal::BasicStream* __os = __og.os();
        config.__write(__os);
    }
    catch(const ::Ice::LocalException& __ex)
    {
        __og.abort(__ex);
    }
    bool __ok = __og.invoke();
    try
    {
        if(!__ok)
        {
            try
            {
                __og.throwUserException();
            }
            catch(const ::ecc::NodeAlreadyExists&)
            {
                throw;
            }
            catch(const ::Ice::UserException& __ex)
            {
                ::Ice::UnknownUserException __uue(__FILE__, __LINE__, __ex.ice_name());
                throw __uue;
            }
        }
        __og.is()->skipEmptyEncaps();
    }
    catch(const ::Ice::LocalException& __ex)
    {
        throw ::IceInternal::LocalExceptionWrapper(__ex, false);
    }
}

void
IceDelegateM::ecc::ElectronicsControlCore::getConfig(::ecc::NodeConfigList& nodes, const ::Ice::Context* __context)
{
    ::IceInternal::Outgoing __og(__handler.get(), __ecc__ElectronicsControlCore__getConfig_name, ::Ice::Idempotent, __context);
    bool __ok = __og.invoke();
    try
    {
        if(!__ok)
        {
            try
            {
                __og.throwUserException();
            }
            catch(const ::Ice::UserException& __ex)
            {
                ::Ice::UnknownUserException __uue(__FILE__, __LINE__, __ex.ice_name());
                throw __uue;
            }
        }
        ::IceInternal::BasicStream* __is = __og.is();
        __is->startReadEncaps();
        ::ecc::__readNodeConfigList(__is, nodes);
        __is->endReadEncaps();
    }
    catch(const ::Ice::LocalException& __ex)
    {
        throw ::IceInternal::LocalExceptionWrapper(__ex, false);
    }
}

void
IceDelegateM::ecc::ElectronicsControlCore::reconfigureNode(const ::mdaq::hw::server::NodeId& id, const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Context* __context)
{
    ::IceInternal::Outgoing __og(__handler.get(), __ecc__ElectronicsControlCore__reconfigureNode_name, ::Ice::Idempotent, __context);
    try
    {
        ::IceInternal::BasicStream* __os = __og.os();
        id.__write(__os);
        config.__write(__os);
    }
    catch(const ::Ice::LocalException& __ex)
    {
        __og.abort(__ex);
    }
    bool __ok = __og.invoke();
    try
    {
        if(!__ok)
        {
            try
            {
                __og.throwUserException();
            }
            catch(const ::ecc::ReconfigFailure&)
            {
                throw;
            }
            catch(const ::Ice::UserException& __ex)
            {
                ::Ice::UnknownUserException __uue(__FILE__, __LINE__, __ex.ice_name());
                throw __uue;
            }
        }
        __og.is()->skipEmptyEncaps();
    }
    catch(const ::Ice::LocalException& __ex)
    {
        throw ::IceInternal::LocalExceptionWrapper(__ex, false);
    }
}

void
IceDelegateM::ecc::ElectronicsControlCore::removeNode(const ::mdaq::hw::server::NodeId& id, const ::Ice::Context* __context)
{
    ::IceInternal::Outgoing __og(__handler.get(), __ecc__ElectronicsControlCore__removeNode_name, ::Ice::Idempotent, __context);
    try
    {
        ::IceInternal::BasicStream* __os = __og.os();
        id.__write(__os);
    }
    catch(const ::Ice::LocalException& __ex)
    {
        __og.abort(__ex);
    }
    bool __ok = __og.invoke();
    if(!__og.is()->b.empty())
    {
        try
        {
            if(!__ok)
            {
                try
                {
                    __og.throwUserException();
                }
                catch(const ::Ice::UserException& __ex)
                {
                    ::Ice::UnknownUserException __uue(__FILE__, __LINE__, __ex.ice_name());
                    throw __uue;
                }
            }
            __og.is()->skipEmptyEncaps();
        }
        catch(const ::Ice::LocalException& __ex)
        {
            throw ::IceInternal::LocalExceptionWrapper(__ex, false);
        }
    }
}

void
IceDelegateD::ecc::ElectronicsControlCore::test(const ::std::string& testString, const ::Ice::Context* __context)
{
    class _DirectI : public ::IceInternal::Direct
    {
    public:

        _DirectI(const ::std::string& testString, const ::Ice::Current& __current) : 
            ::IceInternal::Direct(__current),
            _m_testString(testString)
        {
        }
        
        virtual ::Ice::DispatchStatus
        run(::Ice::Object* object)
        {
            ::ecc::ElectronicsControlCore* servant = dynamic_cast< ::ecc::ElectronicsControlCore*>(object);
            if(!servant)
            {
                throw ::Ice::OperationNotExistException(__FILE__, __LINE__, _current.id, _current.facet, _current.operation);
            }
            servant->test(_m_testString, _current);
            return ::Ice::DispatchOK;
        }
        
    private:
        
        const ::std::string& _m_testString;
    };
    
    ::Ice::Current __current;
    __initCurrent(__current, __ecc__ElectronicsControlCore__test_name, ::Ice::Idempotent, __context);
    try
    {
        _DirectI __direct(testString, __current);
        try
        {
            __direct.servant()->__collocDispatch(__direct);
        }
        catch(...)
        {
            __direct.destroy();
            throw;
        }
        __direct.destroy();
    }
    catch(const ::Ice::SystemException&)
    {
        throw;
    }
    catch(const ::IceInternal::LocalExceptionWrapper&)
    {
        throw;
    }
    catch(const ::std::exception& __ex)
    {
        ::IceInternal::LocalExceptionWrapper::throwWrapper(__ex);
    }
    catch(...)
    {
        throw ::IceInternal::LocalExceptionWrapper(::Ice::UnknownException(__FILE__, __LINE__, "unknown c++ exception"), false);
    }
}

::Ice::Long
IceDelegateD::ecc::ElectronicsControlCore::timeStamp(const ::Ice::Context* __context)
{
    class _DirectI : public ::IceInternal::Direct
    {
    public:

        _DirectI(::Ice::Long& __result, const ::Ice::Current& __current) : 
            ::IceInternal::Direct(__current),
            _result(__result)
        {
        }
        
        virtual ::Ice::DispatchStatus
        run(::Ice::Object* object)
        {
            ::ecc::ElectronicsControlCore* servant = dynamic_cast< ::ecc::ElectronicsControlCore*>(object);
            if(!servant)
            {
                throw ::Ice::OperationNotExistException(__FILE__, __LINE__, _current.id, _current.facet, _current.operation);
            }
            _result = servant->timeStamp(_current);
            return ::Ice::DispatchOK;
        }
        
    private:
        
        ::Ice::Long& _result;
    };
    
    ::Ice::Current __current;
    __initCurrent(__current, __ecc__ElectronicsControlCore__timeStamp_name, ::Ice::Normal, __context);
    ::Ice::Long __result;
    try
    {
        _DirectI __direct(__result, __current);
        try
        {
            __direct.servant()->__collocDispatch(__direct);
        }
        catch(...)
        {
            __direct.destroy();
            throw;
        }
        __direct.destroy();
    }
    catch(const ::Ice::SystemException&)
    {
        throw;
    }
    catch(const ::IceInternal::LocalExceptionWrapper&)
    {
        throw;
    }
    catch(const ::std::exception& __ex)
    {
        ::IceInternal::LocalExceptionWrapper::throwWrapper(__ex);
    }
    catch(...)
    {
        throw ::IceInternal::LocalExceptionWrapper(::Ice::UnknownException(__FILE__, __LINE__, "unknown c++ exception"), false);
    }
    return __result;
}

void
IceDelegateD::ecc::ElectronicsControlCore::createNode(const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Context* __context)
{
    class _DirectI : public ::IceInternal::Direct
    {
    public:

        _DirectI(const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Current& __current) : 
            ::IceInternal::Direct(__current),
            _m_config(config)
        {
        }
        
        virtual ::Ice::DispatchStatus
        run(::Ice::Object* object)
        {
            ::ecc::ElectronicsControlCore* servant = dynamic_cast< ::ecc::ElectronicsControlCore*>(object);
            if(!servant)
            {
                throw ::Ice::OperationNotExistException(__FILE__, __LINE__, _current.id, _current.facet, _current.operation);
            }
            try
            {
                servant->createNode(_m_config, _current);
                return ::Ice::DispatchOK;
            }
            catch(const ::Ice::UserException& __ex)
            {
                setUserException(__ex);
                return ::Ice::DispatchUserException;
            }
        }
        
    private:
        
        const ::mdaq::hw::server::NodeConfig& _m_config;
    };
    
    ::Ice::Current __current;
    __initCurrent(__current, __ecc__ElectronicsControlCore__createNode_name, ::Ice::Idempotent, __context);
    try
    {
        _DirectI __direct(config, __current);
        try
        {
            __direct.servant()->__collocDispatch(__direct);
        }
        catch(...)
        {
            __direct.destroy();
            throw;
        }
        __direct.destroy();
    }
    catch(const ::ecc::NodeAlreadyExists&)
    {
        throw;
    }
    catch(const ::Ice::SystemException&)
    {
        throw;
    }
    catch(const ::IceInternal::LocalExceptionWrapper&)
    {
        throw;
    }
    catch(const ::std::exception& __ex)
    {
        ::IceInternal::LocalExceptionWrapper::throwWrapper(__ex);
    }
    catch(...)
    {
        throw ::IceInternal::LocalExceptionWrapper(::Ice::UnknownException(__FILE__, __LINE__, "unknown c++ exception"), false);
    }
}

void
IceDelegateD::ecc::ElectronicsControlCore::getConfig(::ecc::NodeConfigList& nodes, const ::Ice::Context* __context)
{
    class _DirectI : public ::IceInternal::Direct
    {
    public:

        _DirectI(::ecc::NodeConfigList& nodes, const ::Ice::Current& __current) : 
            ::IceInternal::Direct(__current),
            _m_nodes(nodes)
        {
        }
        
        virtual ::Ice::DispatchStatus
        run(::Ice::Object* object)
        {
            ::ecc::ElectronicsControlCore* servant = dynamic_cast< ::ecc::ElectronicsControlCore*>(object);
            if(!servant)
            {
                throw ::Ice::OperationNotExistException(__FILE__, __LINE__, _current.id, _current.facet, _current.operation);
            }
            servant->getConfig(_m_nodes, _current);
            return ::Ice::DispatchOK;
        }
        
    private:
        
        ::ecc::NodeConfigList& _m_nodes;
    };
    
    ::Ice::Current __current;
    __initCurrent(__current, __ecc__ElectronicsControlCore__getConfig_name, ::Ice::Idempotent, __context);
    try
    {
        _DirectI __direct(nodes, __current);
        try
        {
            __direct.servant()->__collocDispatch(__direct);
        }
        catch(...)
        {
            __direct.destroy();
            throw;
        }
        __direct.destroy();
    }
    catch(const ::Ice::SystemException&)
    {
        throw;
    }
    catch(const ::IceInternal::LocalExceptionWrapper&)
    {
        throw;
    }
    catch(const ::std::exception& __ex)
    {
        ::IceInternal::LocalExceptionWrapper::throwWrapper(__ex);
    }
    catch(...)
    {
        throw ::IceInternal::LocalExceptionWrapper(::Ice::UnknownException(__FILE__, __LINE__, "unknown c++ exception"), false);
    }
}

void
IceDelegateD::ecc::ElectronicsControlCore::reconfigureNode(const ::mdaq::hw::server::NodeId& id, const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Context* __context)
{
    class _DirectI : public ::IceInternal::Direct
    {
    public:

        _DirectI(const ::mdaq::hw::server::NodeId& id, const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Current& __current) : 
            ::IceInternal::Direct(__current),
            _m_id(id),
            _m_config(config)
        {
        }
        
        virtual ::Ice::DispatchStatus
        run(::Ice::Object* object)
        {
            ::ecc::ElectronicsControlCore* servant = dynamic_cast< ::ecc::ElectronicsControlCore*>(object);
            if(!servant)
            {
                throw ::Ice::OperationNotExistException(__FILE__, __LINE__, _current.id, _current.facet, _current.operation);
            }
            try
            {
                servant->reconfigureNode(_m_id, _m_config, _current);
                return ::Ice::DispatchOK;
            }
            catch(const ::Ice::UserException& __ex)
            {
                setUserException(__ex);
                return ::Ice::DispatchUserException;
            }
        }
        
    private:
        
        const ::mdaq::hw::server::NodeId& _m_id;
        const ::mdaq::hw::server::NodeConfig& _m_config;
    };
    
    ::Ice::Current __current;
    __initCurrent(__current, __ecc__ElectronicsControlCore__reconfigureNode_name, ::Ice::Idempotent, __context);
    try
    {
        _DirectI __direct(id, config, __current);
        try
        {
            __direct.servant()->__collocDispatch(__direct);
        }
        catch(...)
        {
            __direct.destroy();
            throw;
        }
        __direct.destroy();
    }
    catch(const ::ecc::ReconfigFailure&)
    {
        throw;
    }
    catch(const ::Ice::SystemException&)
    {
        throw;
    }
    catch(const ::IceInternal::LocalExceptionWrapper&)
    {
        throw;
    }
    catch(const ::std::exception& __ex)
    {
        ::IceInternal::LocalExceptionWrapper::throwWrapper(__ex);
    }
    catch(...)
    {
        throw ::IceInternal::LocalExceptionWrapper(::Ice::UnknownException(__FILE__, __LINE__, "unknown c++ exception"), false);
    }
}

void
IceDelegateD::ecc::ElectronicsControlCore::removeNode(const ::mdaq::hw::server::NodeId& id, const ::Ice::Context* __context)
{
    class _DirectI : public ::IceInternal::Direct
    {
    public:

        _DirectI(const ::mdaq::hw::server::NodeId& id, const ::Ice::Current& __current) : 
            ::IceInternal::Direct(__current),
            _m_id(id)
        {
        }
        
        virtual ::Ice::DispatchStatus
        run(::Ice::Object* object)
        {
            ::ecc::ElectronicsControlCore* servant = dynamic_cast< ::ecc::ElectronicsControlCore*>(object);
            if(!servant)
            {
                throw ::Ice::OperationNotExistException(__FILE__, __LINE__, _current.id, _current.facet, _current.operation);
            }
            servant->removeNode(_m_id, _current);
            return ::Ice::DispatchOK;
        }
        
    private:
        
        const ::mdaq::hw::server::NodeId& _m_id;
    };
    
    ::Ice::Current __current;
    __initCurrent(__current, __ecc__ElectronicsControlCore__removeNode_name, ::Ice::Idempotent, __context);
    try
    {
        _DirectI __direct(id, __current);
        try
        {
            __direct.servant()->__collocDispatch(__direct);
        }
        catch(...)
        {
            __direct.destroy();
            throw;
        }
        __direct.destroy();
    }
    catch(const ::Ice::SystemException&)
    {
        throw;
    }
    catch(const ::IceInternal::LocalExceptionWrapper&)
    {
        throw;
    }
    catch(const ::std::exception& __ex)
    {
        ::IceInternal::LocalExceptionWrapper::throwWrapper(__ex);
    }
    catch(...)
    {
        throw ::IceInternal::LocalExceptionWrapper(::Ice::UnknownException(__FILE__, __LINE__, "unknown c++ exception"), false);
    }
}

::Ice::ObjectPtr
ecc::ElectronicsControlCore::ice_clone() const
{
    throw ::Ice::CloneNotImplementedException(__FILE__, __LINE__);
    return 0; // to avoid a warning with some compilers
}

static const ::std::string __ecc__ElectronicsControlCore_ids[2] =
{
    "::Ice::Object",
    "::ecc::ElectronicsControlCore"
};

bool
ecc::ElectronicsControlCore::ice_isA(const ::std::string& _s, const ::Ice::Current&) const
{
    return ::std::binary_search(__ecc__ElectronicsControlCore_ids, __ecc__ElectronicsControlCore_ids + 2, _s);
}

::std::vector< ::std::string>
ecc::ElectronicsControlCore::ice_ids(const ::Ice::Current&) const
{
    return ::std::vector< ::std::string>(&__ecc__ElectronicsControlCore_ids[0], &__ecc__ElectronicsControlCore_ids[2]);
}

const ::std::string&
ecc::ElectronicsControlCore::ice_id(const ::Ice::Current&) const
{
    return __ecc__ElectronicsControlCore_ids[1];
}

const ::std::string&
ecc::ElectronicsControlCore::ice_staticId()
{
    return __ecc__ElectronicsControlCore_ids[1];
}

::Ice::DispatchStatus
ecc::ElectronicsControlCore::___test(::IceInternal::Incoming& __inS, const ::Ice::Current& __current)
{
    __checkMode(::Ice::Idempotent, __current.mode);
    ::IceInternal::BasicStream* __is = __inS.is();
    __is->startReadEncaps();
    ::std::string testString;
    __is->read(testString);
    __is->endReadEncaps();
    test(testString, __current);
    return ::Ice::DispatchOK;
}

::Ice::DispatchStatus
ecc::ElectronicsControlCore::___timeStamp(::IceInternal::Incoming& __inS, const ::Ice::Current& __current)
{
    __checkMode(::Ice::Normal, __current.mode);
    __inS.is()->skipEmptyEncaps();
    ::IceInternal::BasicStream* __os = __inS.os();
    ::Ice::Long __ret = timeStamp(__current);
    __os->write(__ret);
    return ::Ice::DispatchOK;
}

::Ice::DispatchStatus
ecc::ElectronicsControlCore::___createNode(::IceInternal::Incoming& __inS, const ::Ice::Current& __current)
{
    __checkMode(::Ice::Idempotent, __current.mode);
    ::IceInternal::BasicStream* __is = __inS.is();
    __is->startReadEncaps();
    ::mdaq::hw::server::NodeConfig config;
    config.__read(__is);
    __is->endReadEncaps();
    ::IceInternal::BasicStream* __os = __inS.os();
    try
    {
        createNode(config, __current);
    }
    catch(const ::ecc::NodeAlreadyExists& __ex)
    {
        __os->write(__ex);
        return ::Ice::DispatchUserException;
    }
    return ::Ice::DispatchOK;
}

::Ice::DispatchStatus
ecc::ElectronicsControlCore::___getConfig(::IceInternal::Incoming& __inS, const ::Ice::Current& __current)
{
    __checkMode(::Ice::Idempotent, __current.mode);
    __inS.is()->skipEmptyEncaps();
    ::IceInternal::BasicStream* __os = __inS.os();
    ::ecc::NodeConfigList nodes;
    getConfig(nodes, __current);
    if(nodes.size() == 0)
    {
        __os->writeSize(0);
    }
    else
    {
        ::ecc::__writeNodeConfigList(__os, &nodes[0], &nodes[0] + nodes.size());
    }
    return ::Ice::DispatchOK;
}

::Ice::DispatchStatus
ecc::ElectronicsControlCore::___reconfigureNode(::IceInternal::Incoming& __inS, const ::Ice::Current& __current)
{
    __checkMode(::Ice::Idempotent, __current.mode);
    ::IceInternal::BasicStream* __is = __inS.is();
    __is->startReadEncaps();
    ::mdaq::hw::server::NodeId id;
    ::mdaq::hw::server::NodeConfig config;
    id.__read(__is);
    config.__read(__is);
    __is->endReadEncaps();
    ::IceInternal::BasicStream* __os = __inS.os();
    try
    {
        reconfigureNode(id, config, __current);
    }
    catch(const ::ecc::ReconfigFailure& __ex)
    {
        __os->write(__ex);
        return ::Ice::DispatchUserException;
    }
    return ::Ice::DispatchOK;
}

::Ice::DispatchStatus
ecc::ElectronicsControlCore::___removeNode(::IceInternal::Incoming& __inS, const ::Ice::Current& __current)
{
    __checkMode(::Ice::Idempotent, __current.mode);
    ::IceInternal::BasicStream* __is = __inS.is();
    __is->startReadEncaps();
    ::mdaq::hw::server::NodeId id;
    id.__read(__is);
    __is->endReadEncaps();
    removeNode(id, __current);
    return ::Ice::DispatchOK;
}

static ::std::string __ecc__ElectronicsControlCore_all[] =
{
    "createNode",
    "getConfig",
    "ice_id",
    "ice_ids",
    "ice_isA",
    "ice_ping",
    "reconfigureNode",
    "removeNode",
    "test",
    "timeStamp"
};

::Ice::DispatchStatus
ecc::ElectronicsControlCore::__dispatch(::IceInternal::Incoming& in, const ::Ice::Current& current)
{
    ::std::pair< ::std::string*, ::std::string*> r = ::std::equal_range(__ecc__ElectronicsControlCore_all, __ecc__ElectronicsControlCore_all + 10, current.operation);
    if(r.first == r.second)
    {
        throw ::Ice::OperationNotExistException(__FILE__, __LINE__, current.id, current.facet, current.operation);
    }

    switch(r.first - __ecc__ElectronicsControlCore_all)
    {
        case 0:
        {
            return ___createNode(in, current);
        }
        case 1:
        {
            return ___getConfig(in, current);
        }
        case 2:
        {
            return ___ice_id(in, current);
        }
        case 3:
        {
            return ___ice_ids(in, current);
        }
        case 4:
        {
            return ___ice_isA(in, current);
        }
        case 5:
        {
            return ___ice_ping(in, current);
        }
        case 6:
        {
            return ___reconfigureNode(in, current);
        }
        case 7:
        {
            return ___removeNode(in, current);
        }
        case 8:
        {
            return ___test(in, current);
        }
        case 9:
        {
            return ___timeStamp(in, current);
        }
    }

    assert(false);
    throw ::Ice::OperationNotExistException(__FILE__, __LINE__, current.id, current.facet, current.operation);
}

void
ecc::ElectronicsControlCore::__write(::IceInternal::BasicStream* __os) const
{
    __os->writeTypeId(ice_staticId());
    __os->startWriteSlice();
    __os->endWriteSlice();
#if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
    Object::__write(__os);
#else
    ::Ice::Object::__write(__os);
#endif
}

void
ecc::ElectronicsControlCore::__read(::IceInternal::BasicStream* __is, bool __rid)
{
    if(__rid)
    {
        ::std::string myId;
        __is->readTypeId(myId);
    }
    __is->startReadSlice();
    __is->endReadSlice();
#if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
    Object::__read(__is, true);
#else
    ::Ice::Object::__read(__is, true);
#endif
}

void
ecc::ElectronicsControlCore::__write(const ::Ice::OutputStreamPtr&) const
{
    Ice::MarshalException ex(__FILE__, __LINE__);
    ex.reason = "type ecc::ElectronicsControlCore was not generated with stream support";
    throw ex;
}

void
ecc::ElectronicsControlCore::__read(const ::Ice::InputStreamPtr&, bool)
{
    Ice::MarshalException ex(__FILE__, __LINE__);
    ex.reason = "type ecc::ElectronicsControlCore was not generated with stream support";
    throw ex;
}

void 
ecc::__patch__ElectronicsControlCorePtr(void* __addr, ::Ice::ObjectPtr& v)
{
    ::ecc::ElectronicsControlCorePtr* p = static_cast< ::ecc::ElectronicsControlCorePtr*>(__addr);
    assert(p);
    *p = ::ecc::ElectronicsControlCorePtr::dynamicCast(v);
    if(v && !*p)
    {
        IceInternal::Ex::throwUOE(::ecc::ElectronicsControlCore::ice_staticId(), v->ice_id());
    }
}

bool
ecc::operator==(const ::ecc::ElectronicsControlCore& l, const ::ecc::ElectronicsControlCore& r)
{
    return static_cast<const ::Ice::Object&>(l) == static_cast<const ::Ice::Object&>(r);
}

bool
ecc::operator<(const ::ecc::ElectronicsControlCore& l, const ::ecc::ElectronicsControlCore& r)
{
    return static_cast<const ::Ice::Object&>(l) < static_cast<const ::Ice::Object&>(r);
}
