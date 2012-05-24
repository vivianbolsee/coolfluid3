// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <QFileIconProvider>

#include "ui/core/NRemoteFSBrowser.hpp"

#include "ui/graphics/FileFilter.hpp"

#include <QMimeData>

using namespace cf3::ui::core;

//////////////////////////////////////////////////////////////////////////////

namespace cf3 {
namespace ui {
namespace graphics {

//////////////////////////////////////////////////////////////////////////////

FileFilter::FileFilter( NRemoteFSBrowser * model, QObject * parent )
  : QSortFilterProxyModel( parent ),
    m_model( model )
{
  cf3_assert( is_not_null(model) );

  setSourceModel(m_model);

  connect(m_model,SIGNAL(current_path_changed(QString)),this,SLOT(resort(QString)));
}

//////////////////////////////////////////////////////////////////////////////

QVariant FileFilter::data ( const QModelIndex &index, int role ) const
{
  QVariant value;
  static QFileIconProvider provider; // heavy to initialize, better to put it as static

  if(index.isValid())
  {
    if( role == Qt::DecorationRole && index.column() == 0 )
    {
      QModelIndex indexInModel = mapToSource(index);

      if( m_model->is_directory(indexInModel) )
        value = provider.icon( QFileIconProvider::Folder );
      else if( m_model->is_file(indexInModel) )
        value = provider.icon( QFileIconProvider::File );
    }
    else
      value = QSortFilterProxyModel::data(index, role);
  }

  return value;
}

//////////////////////////////////////////////////////////////////////////////

bool FileFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const{
  if (left.isValid() && right.isValid() && left.column() == 0 && right.column() == 0){
    if (m_model->is_directory(right) && !m_model->is_directory(left)){
      return false;
    }
    QVariant left_data=m_model->data(left,Qt::DisplayRole);
    QVariant right_data=m_model->data(right,Qt::DisplayRole);
    if (left_data.type() == QMetaType::QString && left_data.type() == QMetaType::QString){
      QString left_str(left_data.toString().data());
      QString right_str(right_data.toString().data());
      return right_str.toLower() >= left_str.toLower();
    }
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////////

QMimeData * FileFilter::mimeData(const QModelIndexList &indexes) const
{
  //we don't really care about the data itself, all matter is that this pointer is not null
  return new QMimeData();
}

//////////////////////////////////////////////////////////////////////////////

void FileFilter::resort(QString path){
  sort(0);
}

} // Graphics
} // ui
} // cf3

