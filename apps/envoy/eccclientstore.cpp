#include "eccclientstore_impl_p.h"
#include <utility>
EccClientStore::EccClientStore(const QString &root, const EccSystemClientOptions &options) : fImpl(new Impl(root,options)) {}
EccClientStore::~EccClientStore() {}
