#pragma once

#include "gamemetadata.h"
#include "librarydb.h"
#include "openvgdb.h"
#include "gameimporter.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QDebug>

class GameMetadataModel : public QAbstractTableModel
{
  Q_OBJECT
public:
  explicit GameMetadataModel(QObject* parent = nullptr);
  ~GameMetadataModel() = default;

  enum Roles {
    Title = Qt::UserRole + 1,
    System,
    Description,
    ImageSource,
  };

  QModelIndex createIndexAt(int row, int column) const;

  virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;

  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

  virtual QVariant data(const QModelIndex &index, int role) const override;

  virtual QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE static void doSomething()
  {
    qDebug() << "I am a static function bitch";
  }

  static GameMetadataModel &instance();

public slots:
  virtual void forceUpdate();
  void importGames(QList<QUrl> urls);

private:
  void clearCache();

private:
  QHash<int, QByteArray> roles;
  QVector<GameMetadata> gameMetadataCache;

  LibraryDb libraryDb;
  OpenVgDb openVgDb;
  GameImporter gameImporter;
};
