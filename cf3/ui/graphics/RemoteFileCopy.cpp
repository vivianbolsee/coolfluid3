// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include "ui/graphics/RemoteFileCopy.hpp"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QLabel>
#include <QFileSystemModel>
#include <QMessageBox>
#include <QSplitter>
#include <QProgressBar>

#include "ui/core/ThreadManager.hpp"
#include "ui/core/NBrowser.hpp"
#include "ui/core/TreeThread.hpp"
#include "ui/graphics/FileFilter.hpp"
#include "ui/core/SSHTunnel.hpp"
#include "ui/core/NRemoteFSBrowser.hpp"
#include "common/XML/SignalFrame.hpp"
#include "ui/core/NetworkQueue.hpp"
#include "ui/uicommon/ComponentNames.hpp"


////////////////////////////////////////////////////////////////////////////////

namespace cf3 {
namespace ui {
namespace graphics {

RemoteFileCopy::RemoteFileCopy(QWidget* parent) : QWidget(parent) {
  local_files=new QFileSystemModel(this);
  remote_files=boost::shared_ptr<core::NRemoteFSBrowser>(
        new core::NRemoteFSBrowser(core::NBrowser::global()->generate_name()));
  remote_files->set_include_no_extensions(true);
  filter_remote_model=new FileFilter(remote_files.get(),this);
  core::ThreadManager::instance().tree().root()->add_node(remote_files);
  local_list_widget=new DraggableListWidget(this);
  remote_list_widget=new DraggableListWidget(this);
  QSplitter* splitter=new QSplitter(this);
  QGroupBox* local_box=new QGroupBox("Local File System",splitter);
  QGroupBox* remote_box=new QGroupBox("Remote File System",splitter);
  QVBoxLayout* local_layout=new QVBoxLayout(local_box);
  QVBoxLayout* remote_layout=new QVBoxLayout(remote_box);
  local_current_dir=new QLabel(this);
  remote_current_dir=new QLabel(this);
  local_layout->addWidget(local_current_dir);
  local_layout->addWidget(local_list_widget);
  remote_layout->addWidget(remote_current_dir);
  remote_layout->addWidget(remote_list_widget);
  local_box->setLayout(local_layout);
  remote_box->setLayout(remote_layout);

  QVBoxLayout* main_layout=new QVBoxLayout(this);
  main_layout->addWidget(splitter);
  current_file_copied=new QLabel(this);
  current_file_copied->setVisible(false);
  current_file_copied->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  main_layout->addWidget(current_file_copied);
  percent_copied=new QProgressBar(this);
  percent_copied->setRange(0,100);
  percent_copied->setVisible(false);
  main_layout->addWidget(percent_copied);

  local_list_widget->set_accepted_source(remote_list_widget);
  remote_list_widget->set_accepted_source(local_list_widget);
  local_list_widget->setModel(local_files);
  remote_list_widget->setModel(filter_remote_model);
  local_files->setRootPath("/");
  local_files->setFilter(QDir::AllEntries);
  local_list_widget->setRootIndex(local_files->index(QApplication::applicationDirPath()));
  local_current_dir->setText(QApplication::applicationDirPath());
  remote_files->open_dir("");

  connect(local_list_widget,SIGNAL(copy_request(QModelIndex,QModelIndexList))
          ,this,SLOT(to_local_copy_requested(QModelIndex,QModelIndexList)));
  connect(remote_list_widget,SIGNAL(copy_request(QModelIndex,QModelIndexList))
          ,this,SLOT(to_remote_copy_requested(QModelIndex,QModelIndexList)));
  connect(local_list_widget,SIGNAL(item_double_clicked(QModelIndex))
          ,this,SLOT(local_item_double_clicked(QModelIndex)));
  connect(remote_list_widget,SIGNAL(item_double_clicked(QModelIndex))
          ,this,SLOT(remote_item_double_clicked(QModelIndex)));
  connect(remote_files.get(),SIGNAL(current_path_changed(const QString &))
          ,remote_current_dir,SLOT(setText(const QString &)));
  connect(remote_files.get(),SIGNAL(copy_finished(int,QString)),this,SLOT(copy_finished(int,QString)));
}

////////////////////////////////////////////////////////////////////////////////

RemoteFileCopy::~RemoteFileCopy(){
  //core::ThreadManager::instance().tree().root()->remove_node(remote_files->name());
  //delete remote_files;
}

////////////////////////////////////////////////////////////////////////////////

void RemoteFileCopy::to_local_copy_requested(QModelIndex item, QModelIndexList copy_items){
  if (!percent_copied->isVisible()){
    std::vector<std::string> params;
    std::vector<std::string> file_names;
    std::string local_signature=core::SSHTunnel::get_local_signature();
    local_signature.append(":");
    if (item.isValid()){
      if (local_files->isDir(item))
        local_signature.append(local_files->filePath(item).toStdString());
      else
        local_signature.append(local_files->filePath(local_files->parent(item)).toStdString());
    }else{
      item=local_list_widget->rootIndex();
      local_signature.append(local_files->filePath(item).toStdString());
    }
    foreach (const QModelIndex & to_copy, copy_items){
      QModelIndex to_copy_not_const = filter_remote_model->mapToSource(to_copy);
      if (remote_files->is_directory(to_copy_not_const))
        params.push_back("-r");
      params.push_back("-p");
      params.push_back(remote_files->retrieve_full_path(to_copy_not_const).toStdString());
      file_names.push_back(remote_files->data(to_copy_not_const,Qt::DisplayRole).toString().toStdString());
      params.push_back(local_signature);
      params.push_back("#");
    }
    remote_files->copy_request(params,file_names);
    percent_copied->setVisible(true);
    percent_copied->setValue(0);
    current_file_copied->setVisible(true);
  }else{
    QMessageBox::critical(this,"Error","Some files are still being copied, please wait that the copy is finished.");
  }
}

////////////////////////////////////////////////////////////////////////////////

void RemoteFileCopy::to_remote_copy_requested(QModelIndex item, QModelIndexList copy_items){
  if (!percent_copied->isVisible()){
    std::string dest;
    std::vector<std::string> params;
    std::vector<std::string> file_names;
    std::string local_signature=core::SSHTunnel::get_local_signature().append(":");
    item = filter_remote_model->mapToSource(item);
    if (item.isValid() && remote_files->is_directory(item))
      dest=remote_files->retrieve_full_path(item).toStdString();
    else
      dest=remote_files->current_path().toStdString();
    foreach (const QModelIndex & to_copy, copy_items){
      if (local_files->isDir(to_copy))
        params.push_back("-r");
      params.push_back("-p");
      params.push_back(std::string(local_signature).append(local_files->filePath(to_copy).toStdString()));
      file_names.push_back(local_files->fileName(to_copy).toStdString());
      params.push_back(dest);
      params.push_back("#");
    }
    remote_files->copy_request(params,file_names);
    percent_copied->setVisible(true);
    percent_copied->setValue(0);
    current_file_copied->setVisible(true);
  }else{
    QMessageBox::critical(this,"Error","Some files are still being copied, please wait that the copy is finished.");
  }
}

////////////////////////////////////////////////////////////////////////////////

void RemoteFileCopy::local_item_double_clicked(QModelIndex item){
  if (local_files->isDir(item)){
    QString local_path=QDir::cleanPath(local_files->filePath(item));
    item=local_files->index(local_path);
    if (item.column() >= 0){
      local_list_widget->setRootIndex(item);
      local_current_dir->setText(local_path);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void RemoteFileCopy::remote_item_double_clicked(QModelIndex item){
  item = filter_remote_model->mapToSource(item);
  if (remote_files->is_directory(item)){
    remote_files->open_dir(QDir::cleanPath(remote_files->retrieve_full_path(item)));
  }
}

////////////////////////////////////////////////////////////////////////////////

void RemoteFileCopy::copy_finished(int percent, QString current_file){
  if (current_file.isEmpty())
    current_file_copied->setText("");
  else
    current_file_copied->setText("Current file copied : "+current_file);
  if (percent == 100){//copy finished
    percent_copied->setVisible(false);
    current_file_copied->setVisible(false);
  }else
    percent_copied->setValue(percent);
  remote_files->open_dir(remote_files->current_path());
}

////////////////////////////////////////////////////////////////////////////////

DraggableListWidget::DraggableListWidget(QWidget* parent)
  : QListView(parent){
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setAcceptDrops(true);
  setDefaultDropAction(Qt::CopyAction);
  setAlternatingRowColors(true);
}

////////////////////////////////////////////////////////////////////////////////

void DraggableListWidget::set_accepted_source(DraggableListWidget* accepted_source){
  this->accepted_source=accepted_source;
}

////////////////////////////////////////////////////////////////////////////////

void DraggableListWidget::mouseDoubleClickEvent(QMouseEvent *e){
  emit item_double_clicked(indexAt(e->pos()));
}

////////////////////////////////////////////////////////////////////////////////

void DraggableListWidget::dropEvent(QDropEvent *e){
  //qDebug() << "dropEvent" ;
  if (e->source() == accepted_source){
    emit copy_request(indexAt(e->pos()),accepted_source->selectedIndexes());
  }
  e->ignore();
}

////////////////////////////////////////////////////////////////////////////////

void DraggableListWidget::dragEnterEvent(QDragEnterEvent *e){
  //qDebug() << "dragEnterEvent" ;
  e->acceptProposedAction();
}

////////////////////////////////////////////////////////////////////////////////

void DraggableListWidget::dragMoveEvent(QDragMoveEvent *e){
  e->acceptProposedAction();
}

////////////////////////////////////////////////////////////////////////////////

void DraggableListWidget::dragLeaveEvent(QDragLeaveEvent *e){
  //qDebug() << "dragLeaveEvent" ;
  e->accept();
}

////////////////////////////////////////////////////////////////////////////////

} // Graphics
} // ui
} // cf3

////////////////////////////////////////////////////////////////////////////////
