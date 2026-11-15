#include "localbackendregistry.h"
#include "applicationinstance.h"
#include <QSettings>
#include <QFileInfo>
void RememberLocalBackend(const QString &configuration) {
    if(configuration.isEmpty())return;
    QSettings settings;const auto key=StartupConfigurationKey()+"/backends";
    auto files=settings.value(key).toStringList();files.removeAll(configuration);files.prepend(configuration);
    while(files.size()>128)files.removeLast();
    settings.setValue(key,files);
}
QStringList KnownLocalBackends() {
    QStringList result;for(const auto &file:QSettings().value(StartupConfigurationKey()+"/backends").toStringList())if(QFileInfo::exists(file))result.append(file);return result;
}
