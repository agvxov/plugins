// Copyright (c) 2023 Manuel Schneider

#pragma once
#include "albert/extension/queryhandler/globalqueryhandler.h"
#include "albert/plugin.h"


class TimeZoneHandler : public albert::TriggerQueryHandler
{
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString synopsis() const override;
    QString defaultTrigger() const override;
    void handleTriggerQuery(TriggerQuery*) const override;
};


class Plugin : public albert::plugin::ExtensionPlugin,
               public albert::GlobalQueryHandler
{
    Q_OBJECT ALBERT_PLUGIN
public:
    QWidget *buildConfigWidget() override;
    std::vector<albert::RankItem> handleGlobalQuery(const GlobalQuery*) const override;
    std::vector<albert::Extension*> extensions() override;

    static const QStringList icon_urls;
private:
    TimeZoneHandler tzh;
};
