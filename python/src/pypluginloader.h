// Copyright (c) 2017-2023 Manuel Schneider

#pragma once
#include "pybind11/pybind11.h"
#include "albert/extension/pluginprovider/pluginmetadata.h"
#include "albert/extension/pluginprovider/pluginloader.h"
#include <memory>
#include <QLoggingCategory>
namespace albert {
class PluginProvider;
class PluginInstance;
}
class QFileInfo;
class Plugin;

class NoPluginException: public std::exception
{
public:
    NoPluginException(const std::string &what): what_(what) {}
    const char *what() const noexcept override { return what_.c_str(); }
private:
    std::string what_;
};

class PyPluginLoader : public albert::PluginLoader
{
public:
    PyPluginLoader(Plugin &provider, const QFileInfo &file_info);
    ~PyPluginLoader();

    const albert::PluginProvider &provider() const override;
    const albert::PluginMetaData &metaData() const override;
    albert::PluginInstance *instance() const override;

    QString load() override;
    QString unload() override;

    const QString &source_path() const;

private:
    QString load_();

    QString source_path_;
    pybind11::module module_;
    Plugin &provider_;
    pybind11::object py_plugin_instance_;
    albert::PluginInstance *cpp_plugin_instance_;
    albert::PluginMetaData metadata_;
    std::string logging_category_name;
    std::unique_ptr<QLoggingCategory> logging_category;
};





