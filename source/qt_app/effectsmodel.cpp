#include "effectsmodel.h"

EffectsModel::EffectsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_effects = {
                  { 0, "Часы (цифры)",  "Отображение текущего времени"     },
                  { 1, "Пульс",         "Пульсирующее свечение"             },
                  { 2, "Радуга",        "Плавный перелив цветов"            },
                  { 3, "Плавные часы",  "Мягкая смена цвета цифр"           },
                  { 4, "Спираль",       "Закрученная анимация вокруг центра" },
                  { 5, "Змейка",        "Бегущие огоньки по матрице"        },
                  { 6, "Снег",          "Медленно падающие снежинки"        },
                  { 7, "Искры",         "Случайные белые вспышки"           },
                  { 8, "Матрица",       "Цифровой дождь"                    },
                  { 9, "Огонь",         "Тёплое мерцание"                   },
                  };
}

int EffectsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_effects.size();
}

QVariant EffectsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    if (index.row() < 0 || index.row() >= m_effects.size()) return {};

    const Effect &e = m_effects.at(index.row());

    switch (role) {
    case Qt::DisplayRole:   return e.name;
    case Qt::ToolTipRole:   return e.description;
    case Qt::UserRole:      return e.id;
    }
    return {};
}

int EffectsModel::effectIdAt(int row) const
{
    if (row < 0 || row >= m_effects.size()) return -1;
    return m_effects.at(row).id;
}

int EffectsModel::rowForEffectId(int id) const
{
    for (int i = 0; i < m_effects.size(); ++i)
        if (m_effects.at(i).id == id) return i;
    return -1;
}