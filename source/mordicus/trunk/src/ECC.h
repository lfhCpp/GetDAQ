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

#ifndef ___home_pzhao_workspace_GWT_ControlCore_src_ECC_h__
#define ___home_pzhao_workspace_GWT_ControlCore_src_ECC_h__

#include <Ice/LocalObjectF.h>
#include <Ice/ProxyF.h>
#include <Ice/ObjectF.h>
#include <Ice/Exception.h>
#include <Ice/LocalObject.h>
#include <Ice/Proxy.h>
#include <Ice/Object.h>
#include <Ice/Outgoing.h>
#include <Ice/Incoming.h>
#include <Ice/Direct.h>
#include <Ice/UserExceptionFactory.h>
#include <Ice/FactoryTable.h>
#include <Ice/StreamF.h>
#include <HwControl.h>
#include <Ice/UndefSysMacros.h>

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

namespace IceProxy
{

namespace ecc
{

class ElectronicsControlCore;

}

}

namespace ecc
{

class ElectronicsControlCore;
bool operator==(const ElectronicsControlCore&, const ElectronicsControlCore&);
bool operator<(const ElectronicsControlCore&, const ElectronicsControlCore&);

}

namespace IceInternal
{

::Ice::Object* upCast(::ecc::ElectronicsControlCore*);
::IceProxy::Ice::Object* upCast(::IceProxy::ecc::ElectronicsControlCore*);

}

namespace ecc
{

typedef ::IceInternal::Handle< ::ecc::ElectronicsControlCore> ElectronicsControlCorePtr;
typedef ::IceInternal::ProxyHandle< ::IceProxy::ecc::ElectronicsControlCore> ElectronicsControlCorePrx;

void __read(::IceInternal::BasicStream*, ElectronicsControlCorePrx&);
void __patch__ElectronicsControlCorePtr(void*, ::Ice::ObjectPtr&);

}

namespace ecc
{

typedef ::std::vector< ::mdaq::hw::server::NodeConfig> NodeConfigList;
void __writeNodeConfigList(::IceInternal::BasicStream*, const ::mdaq::hw::server::NodeConfig*, const ::mdaq::hw::server::NodeConfig*);
void __readNodeConfigList(::IceInternal::BasicStream*, NodeConfigList&);

struct SystemConfig
{
    ::ecc::NodeConfigList nodes;

    bool operator==(const SystemConfig&) const;
    bool operator<(const SystemConfig&) const;
    bool operator!=(const SystemConfig& __rhs) const
    {
        return !operator==(__rhs);
    }
    bool operator<=(const SystemConfig& __rhs) const
    {
        return operator<(__rhs) || operator==(__rhs);
    }
    bool operator>(const SystemConfig& __rhs) const
    {
        return !operator<(__rhs) && !operator==(__rhs);
    }
    bool operator>=(const SystemConfig& __rhs) const
    {
        return !operator<(__rhs);
    }

    void __write(::IceInternal::BasicStream*) const;
    void __read(::IceInternal::BasicStream*);
};

class NodeAlreadyExists : public ::Ice::UserException
{
public:

    NodeAlreadyExists() {}
    explicit NodeAlreadyExists(const ::std::string&);
    virtual ~NodeAlreadyExists() throw();

    virtual ::std::string ice_name() const;
    virtual ::Ice::Exception* ice_clone() const;
    virtual void ice_throw() const;

    static const ::IceInternal::UserExceptionFactoryPtr& ice_factory();

    ::std::string reason;

    virtual void __write(::IceInternal::BasicStream*) const;
    virtual void __read(::IceInternal::BasicStream*, bool);
    virtual void __write(const ::Ice::OutputStreamPtr&) const;
    virtual void __read(const ::Ice::InputStreamPtr&, bool);
};

static NodeAlreadyExists __NodeAlreadyExists_init;

class ReconfigFailure : public ::Ice::UserException
{
public:

    ReconfigFailure() {}
    explicit ReconfigFailure(const ::std::string&);
    virtual ~ReconfigFailure() throw();

    virtual ::std::string ice_name() const;
    virtual ::Ice::Exception* ice_clone() const;
    virtual void ice_throw() const;

    static const ::IceInternal::UserExceptionFactoryPtr& ice_factory();

    ::std::string reason;

    virtual void __write(::IceInternal::BasicStream*) const;
    virtual void __read(::IceInternal::BasicStream*, bool);
    virtual void __write(const ::Ice::OutputStreamPtr&) const;
    virtual void __read(const ::Ice::InputStreamPtr&, bool);
};

}

namespace IceProxy
{

namespace ecc
{

class ElectronicsControlCore : virtual public ::IceProxy::Ice::Object
{
public:

    void test(const ::std::string& testString)
    {
        test(testString, 0);
    }
    void test(const ::std::string& testString, const ::Ice::Context& __ctx)
    {
        test(testString, &__ctx);
    }
    
private:

    void test(const ::std::string&, const ::Ice::Context*);
    
public:

    ::Ice::Long timeStamp()
    {
        return timeStamp(0);
    }
    ::Ice::Long timeStamp(const ::Ice::Context& __ctx)
    {
        return timeStamp(&__ctx);
    }
    
private:

    ::Ice::Long timeStamp(const ::Ice::Context*);
    
public:

    void createNode(const ::mdaq::hw::server::NodeConfig& config)
    {
        createNode(config, 0);
    }
    void createNode(const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Context& __ctx)
    {
        createNode(config, &__ctx);
    }
    
private:

    void createNode(const ::mdaq::hw::server::NodeConfig&, const ::Ice::Context*);
    
public:

    void getConfig(::ecc::NodeConfigList& nodes)
    {
        getConfig(nodes, 0);
    }
    void getConfig(::ecc::NodeConfigList& nodes, const ::Ice::Context& __ctx)
    {
        getConfig(nodes, &__ctx);
    }
    
private:

    void getConfig(::ecc::NodeConfigList&, const ::Ice::Context*);
    
public:

    void reconfigureNode(const ::mdaq::hw::server::NodeId& id, const ::mdaq::hw::server::NodeConfig& config)
    {
        reconfigureNode(id, config, 0);
    }
    void reconfigureNode(const ::mdaq::hw::server::NodeId& id, const ::mdaq::hw::server::NodeConfig& config, const ::Ice::Context& __ctx)
    {
        reconfigureNode(id, config, &__ctx);
    }
    
private:

    void reconfigureNode(const ::mdaq::hw::server::NodeId&, const ::mdaq::hw::server::NodeConfig&, const ::Ice::Context*);
    
public:

    void removeNode(const ::mdaq::hw::server::NodeId& id)
    {
        removeNode(id, 0);
    }
    void removeNode(const ::mdaq::hw::server::NodeId& id, const ::Ice::Context& __ctx)
    {
        removeNode(id, &__ctx);
    }
    
private:

    void removeNode(const ::mdaq::hw::server::NodeId&, const ::Ice::Context*);
    
public:
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_context(const ::Ice::Context& __context) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_context(__context).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_context(__context).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_adapterId(const std::string& __id) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_adapterId(__id).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_adapterId(__id).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_endpoints(const ::Ice::EndpointSeq& __endpoints) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_endpoints(__endpoints).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_endpoints(__endpoints).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_locatorCacheTimeout(int __timeout) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_locatorCacheTimeout(__timeout).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_locatorCacheTimeout(__timeout).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_connectionCached(bool __cached) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_connectionCached(__cached).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_connectionCached(__cached).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_endpointSelection(::Ice::EndpointSelectionType __est) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_endpointSelection(__est).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_endpointSelection(__est).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_secure(bool __secure) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_secure(__secure).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_secure(__secure).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_preferSecure(bool __preferSecure) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_preferSecure(__preferSecure).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_preferSecure(__preferSecure).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_router(const ::Ice::RouterPrx& __router) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_router(__router).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_router(__router).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_locator(const ::Ice::LocatorPrx& __locator) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_locator(__locator).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_locator(__locator).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_collocationOptimized(bool __co) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_collocationOptimized(__co).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_collocationOptimized(__co).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_twoway() const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_twoway().get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_twoway().get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_oneway() const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_oneway().get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_oneway().get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_batchOneway() const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_batchOneway().get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_batchOneway().get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_datagram() const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_datagram().get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_datagram().get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_batchDatagram() const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_batchDatagram().get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_batchDatagram().get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_compress(bool __compress) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_compress(__compress).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_compress(__compress).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_timeout(int __timeout) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_timeout(__timeout).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_timeout(__timeout).get());
    #endif
    }
    
    ::IceInternal::ProxyHandle<ElectronicsControlCore> ice_connectionId(const std::string& __id) const
    {
    #if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug
        typedef ::IceProxy::Ice::Object _Base;
        return dynamic_cast<ElectronicsControlCore*>(_Base::ice_connectionId(__id).get());
    #else
        return dynamic_cast<ElectronicsControlCore*>(::IceProxy::Ice::Object::ice_connectionId(__id).get());
    #endif
    }
    
    static const ::std::string& ice_staticId();

private: 

    virtual ::IceInternal::Handle< ::IceDelegateM::Ice::Object> __createDelegateM();
    virtual ::IceInternal::Handle< ::IceDelegateD::Ice::Object> __createDelegateD();
    virtual ::IceProxy::Ice::Object* __newInstance() const;
};

}

}

namespace IceDelegate
{

namespace ecc
{

class ElectronicsControlCore : virtual public ::IceDelegate::Ice::Object
{
public:

    virtual void test(const ::std::string&, const ::Ice::Context*) = 0;

    virtual ::Ice::Long timeStamp(const ::Ice::Context*) = 0;

    virtual void createNode(const ::mdaq::hw::server::NodeConfig&, const ::Ice::Context*) = 0;

    virtual void getConfig(::ecc::NodeConfigList&, const ::Ice::Context*) = 0;

    virtual void reconfigureNode(const ::mdaq::hw::server::NodeId&, const ::mdaq::hw::server::NodeConfig&, const ::Ice::Context*) = 0;

    virtual void removeNode(const ::mdaq::hw::server::NodeId&, const ::Ice::Context*) = 0;
};

}

}

namespace IceDelegateM
{

namespace ecc
{

class ElectronicsControlCore : virtual public ::IceDelegate::ecc::ElectronicsControlCore,
                               virtual public ::IceDelegateM::Ice::Object
{
public:

    virtual void test(const ::std::string&, const ::Ice::Context*);

    virtual ::Ice::Long timeStamp(const ::Ice::Context*);

    virtual void createNode(const ::mdaq::hw::server::NodeConfig&, const ::Ice::Context*);

    virtual void getConfig(::ecc::NodeConfigList&, const ::Ice::Context*);

    virtual void reconfigureNode(const ::mdaq::hw::server::NodeId&, const ::mdaq::hw::server::NodeConfig&, const ::Ice::Context*);

    virtual void removeNode(const ::mdaq::hw::server::NodeId&, const ::Ice::Context*);
};

}

}

namespace IceDelegateD
{

namespace ecc
{

class ElectronicsControlCore : virtual public ::IceDelegate::ecc::ElectronicsControlCore,
                               virtual public ::IceDelegateD::Ice::Object
{
public:

    virtual void test(const ::std::string&, const ::Ice::Context*);

    virtual ::Ice::Long timeStamp(const ::Ice::Context*);

    virtual void createNode(const ::mdaq::hw::server::NodeConfig&, const ::Ice::Context*);

    virtual void getConfig(::ecc::NodeConfigList&, const ::Ice::Context*);

    virtual void reconfigureNode(const ::mdaq::hw::server::NodeId&, const ::mdaq::hw::server::NodeConfig&, const ::Ice::Context*);

    virtual void removeNode(const ::mdaq::hw::server::NodeId&, const ::Ice::Context*);
};

}

}

namespace ecc
{

class ElectronicsControlCore : virtual public ::Ice::Object
{
public:

    typedef ElectronicsControlCorePrx ProxyType;
    typedef ElectronicsControlCorePtr PointerType;
    
    virtual ::Ice::ObjectPtr ice_clone() const;

    virtual bool ice_isA(const ::std::string&, const ::Ice::Current& = ::Ice::Current()) const;
    virtual ::std::vector< ::std::string> ice_ids(const ::Ice::Current& = ::Ice::Current()) const;
    virtual const ::std::string& ice_id(const ::Ice::Current& = ::Ice::Current()) const;
    static const ::std::string& ice_staticId();

    virtual void test(const ::std::string&, const ::Ice::Current& = ::Ice::Current()) = 0;
    ::Ice::DispatchStatus ___test(::IceInternal::Incoming&, const ::Ice::Current&);

    virtual ::Ice::Long timeStamp(const ::Ice::Current& = ::Ice::Current()) = 0;
    ::Ice::DispatchStatus ___timeStamp(::IceInternal::Incoming&, const ::Ice::Current&);

    virtual void createNode(const ::mdaq::hw::server::NodeConfig&, const ::Ice::Current& = ::Ice::Current()) = 0;
    ::Ice::DispatchStatus ___createNode(::IceInternal::Incoming&, const ::Ice::Current&);

    virtual void getConfig(::ecc::NodeConfigList&, const ::Ice::Current& = ::Ice::Current()) = 0;
    ::Ice::DispatchStatus ___getConfig(::IceInternal::Incoming&, const ::Ice::Current&);

    virtual void reconfigureNode(const ::mdaq::hw::server::NodeId&, const ::mdaq::hw::server::NodeConfig&, const ::Ice::Current& = ::Ice::Current()) = 0;
    ::Ice::DispatchStatus ___reconfigureNode(::IceInternal::Incoming&, const ::Ice::Current&);

    virtual void removeNode(const ::mdaq::hw::server::NodeId&, const ::Ice::Current& = ::Ice::Current()) = 0;
    ::Ice::DispatchStatus ___removeNode(::IceInternal::Incoming&, const ::Ice::Current&);

    virtual ::Ice::DispatchStatus __dispatch(::IceInternal::Incoming&, const ::Ice::Current&);

    virtual void __write(::IceInternal::BasicStream*) const;
    virtual void __read(::IceInternal::BasicStream*, bool);
    virtual void __write(const ::Ice::OutputStreamPtr&) const;
    virtual void __read(const ::Ice::InputStreamPtr&, bool);
};

}

#endif
