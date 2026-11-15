#include "configvaluedelegate_p.h"
#include "configurationrange.h"

#include <QComboBox>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <limits>
#include <utility>

ConfigValueDelegate::ConfigValueDelegate(const XcfgDocument *document, Commit commit,
                                         QObject *parent)
    : QStyledItemDelegate(parent), fDocument(document), fCommit(std::move(commit))
{
}

QWidget *ConfigValueDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
                                           const QModelIndex &index) const
{
    if (index.column() != 1 || !fDocument) return nullptr;
    const int id = index.sibling(index.row(), 0).data(Qt::UserRole).toInt();
    if (id < 0 || id >= fDocument->Nodes().size()) return nullptr;
    const auto &node = fDocument->Nodes()[id];
    if (!node.scalar || !node.valueEditable) return nullptr;

    GetDAQ::ConfigurationValue parsed;
    try {
        parsed = GetDAQ::ParseConfigurationValue(node.value.toStdString(),
                                                  node.attributes.value("type").toStdString());
        const auto text = node.attributes.value("range").trimmed();
        if (!text.isEmpty()) {
            const auto range = GetDAQ::ParseConfigurationRange(text.toStdString());
            if (range.kind == GetDAQ::ConfigurationRange::Enumeration) {
                auto combo = new QComboBox(parent);
                for (const auto &value : range.values)
                    combo->addItem(QString::fromStdString(GetDAQ::NormalizeConfigurationValue(value)));
                if (combo->findText(node.value, Qt::MatchFixedString) < 0) combo->addItem(node.value);
                return combo;
            }
        }
    } catch (...) {
        // Keep malformed declarations editable; the document validator reports source errors.
    }

    if (parsed.type == GetDAQ::ConfigurationValue::Boolean) {
        auto combo = new QComboBox(parent);
        bool addFalse = true, addTrue = true;
        try {
            const auto text = node.attributes.value("range").trimmed();
            if (!text.isEmpty()) {
                const auto range = GetDAQ::ParseConfigurationRange(text.toStdString());
                if (range.kind == GetDAQ::ConfigurationRange::Interval) {
                    addFalse = range.Contains(GetDAQ::ParseConfigurationValue("false", "bool"));
                    addTrue = range.Contains(GetDAQ::ParseConfigurationValue("true", "bool"));
                }
            }
        } catch (...) {
        }
        if (addFalse) combo->addItem(QStringLiteral("false"));
        if (addTrue) combo->addItem(QStringLiteral("true"));
        if (combo->findText(node.value, Qt::MatchFixedString) < 0) combo->addItem(node.value);
        return combo;
    }

    if (parsed.type == GetDAQ::ConfigurationValue::Integer) {
        int minimum = std::numeric_limits<int>::min();
        int maximum = std::numeric_limits<int>::max();
        bool usable = true;
        try {
            const auto text = node.attributes.value("range").trimmed();
            if (!text.isEmpty()) {
                const auto range = GetDAQ::ParseConfigurationRange(text.toStdString());
                if (range.kind == GetDAQ::ConfigurationRange::Interval) {
                    if (!range.lowerUnbounded) {
                        qint64 bound = range.lower.integer;
                        if (!range.includeLower && bound == std::numeric_limits<qint64>::max()) usable = false;
                        else if (!range.includeLower) ++bound;
                        if (bound < std::numeric_limits<int>::min() || bound > std::numeric_limits<int>::max()) usable = false;
                        else minimum = static_cast<int>(bound);
                    }
                    if (!range.upperUnbounded) {
                        qint64 bound = range.upper.integer;
                        if (!range.includeUpper && bound == std::numeric_limits<qint64>::min()) usable = false;
                        else if (!range.includeUpper) --bound;
                        if (bound < std::numeric_limits<int>::min() || bound > std::numeric_limits<int>::max()) usable = false;
                        else maximum = static_cast<int>(bound);
                    }
                }
            }
        } catch (...) {
            usable = false;
        }
        if (usable && minimum <= maximum) {
            auto spin = new QSpinBox(parent);
            spin->setRange(minimum, maximum);
            return spin;
        }
    }

    auto line = new QLineEdit(parent);
    if (parsed.type == GetDAQ::ConfigurationValue::Integer)
        line->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[+-]?[0-9]*")), line));
    else if (parsed.type == GetDAQ::ConfigurationValue::Real)
        line->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?")), line));
    else if (parsed.type == GetDAQ::ConfigurationValue::Hexadecimal)
        line->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("0[xX][0-9A-Fa-f]{1,16}")), line));
    return line;
}

void ConfigValueDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const QString value = index.data(Qt::EditRole).toString();
    if (auto combo = qobject_cast<QComboBox *>(editor)) {
        const int i = combo->findText(value, Qt::MatchFixedString);
        if (i >= 0) combo->setCurrentIndex(i);
    } else if (auto spin = qobject_cast<QSpinBox *>(editor)) {
        bool ok = false;
        const int n = value.toInt(&ok);
        if (ok) {
            spin->setRange(qMin(spin->minimum(), n), qMax(spin->maximum(), n));
            spin->setValue(n);
        }
    } else if (auto line = qobject_cast<QLineEdit *>(editor)) {
        line->setText(value);
    }
}

void ConfigValueDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                       const QModelIndex &index) const
{
    QString value;
    if (auto combo = qobject_cast<QComboBox *>(editor)) value = combo->currentText();
    else if (auto spin = qobject_cast<QSpinBox *>(editor)) value = QString::number(spin->value());
    else if (auto line = qobject_cast<QLineEdit *>(editor)) value = line->text();
    if (value == index.data(Qt::EditRole).toString()) return;
    const int id = index.sibling(index.row(), 0).data(Qt::UserRole).toInt();
    QString error;
    if (!fCommit || !fCommit(id, value, &error)) {
        if (fError) fError(error);
        return;
    }
    model->setData(index, value, Qt::EditRole);
}

void ConfigValueDelegate::setErrorHandler(std::function<void(const QString &)> handler)
{
    fError = std::move(handler);
}
