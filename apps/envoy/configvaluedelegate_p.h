#ifndef GETDAQ_CONFIGVALUEDELEGATE_P_H
#define GETDAQ_CONFIGVALUEDELEGATE_P_H

#include "xcfgdocument.h"
#include <QStyledItemDelegate>
#include <functional>

class ConfigValueDelegate final : public QStyledItemDelegate {
public:
    using Commit = std::function<bool(int, const QString &, QString *)>;

    ConfigValueDelegate(const XcfgDocument *document, Commit commit, QObject *parent);
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void setErrorHandler(std::function<void(const QString &)> handler);

private:
    const XcfgDocument *fDocument;
    Commit fCommit;
    std::function<void(const QString &)> fError;
};

#endif
