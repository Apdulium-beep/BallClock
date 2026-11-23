#ifndef EFFECTSMODEL_H
#define EFFECTSMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QString>

class EffectsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Effect {
        int     id;
        QString name;
        QString description;
    };

    explicit EffectsModel(QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    int effectIdAt(int row) const;
    int rowForEffectId(int id) const;

private:
    QVector<Effect> m_effects;
};

#endif // EFFECTSMODEL_H