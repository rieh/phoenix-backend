#pragma once

#include <QString>
#include <QDateTime>
#include <QVariantHash>

struct GameEntry {
  int rowIndex{ -1 };
  QString absoluteFilePath;
  QString sha1Checksum;

  QDateTime timePlayed;
  QString gameImageSource;
  QString gameDescription;

  int userSetCore{ -1 };
  int defaultCore{ -1 };

  GameEntry(const QVariantHash &hash)
  {
    rowIndex = hash.value("rowIndex").toInt();
    absoluteFilePath = hash.value("absoluteFilePath").toString();
    sha1Checksum = hash.value("sha1Checksum").toString();

    timePlayed = hash.value("timePlayed").toDateTime();
    gameImageSource = hash.value("gameImageSource").toString();
    gameDescription = hash.value("gameDescription").toString();

    userSetCore = hash.value("userSetCore").toInt();
    defaultCore = hash.value("defaultCore").toInt();
  }

  GameEntry() = default;

  QVariantHash getQueryFriendlyHash()
  {
    QVariantHash result;

    if (!absoluteFilePath.isEmpty()) {
      result.insert("absoluteFilePath", absoluteFilePath);
    }

    if (!sha1Checksum.isEmpty()) {
      result.insert("sha1Checksum", sha1Checksum);
    }

    if (!timePlayed.isNull() && timePlayed.isValid()) {
      result.insert("timePlayed", timePlayed);
    }

    if (!gameImageSource.isEmpty()) {
      result.insert("gameImageSource", gameImageSource);
    }

    if (!gameDescription.isEmpty()) {
      result.insert("gameDescription", gameDescription);
    }

    if (userSetCore != -1) {
      result.insert("userSetCore", userSetCore);
    }

    if (defaultCore != -1) {
      result.insert("defaultCore", defaultCore);
    }

    return result;
  }
};
