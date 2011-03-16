// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <QApplication>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QStatusBar>

#include "Common/URI.hpp"
#include "Common/Signal.hpp"

#include "GUI/Client/Core/NetworkThread.hpp"
#include "GUI/Client/Core/NLog.hpp"
#include "GUI/Client/Core/ThreadManager.hpp"

#include "GUI/Client/UI/SignatureDialog.hpp"

#include "GUI/Client/UI/SignalManager.hpp"

//////////////////////////////////////////////////////////////////////////////

using namespace CF::Common;
using namespace CF::Common::XML;
using namespace CF::GUI::ClientCore;

//////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace GUI {
namespace ClientUI {

/////////////////////////////////////////////////////////////////////////


SignalManager::SignalManager(QMainWindow *parent) :
    QObject(parent),
    m_currentAction(nullptr),
    m_waitingForSignature(false)
{
  m_menu = new QMenu();
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

SignalManager::~SignalManager()
{
  m_menu->clear();
  delete m_menu;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void SignalManager::showMenu(const QPoint & pos, CNode::Ptr node,
                             const QList<ActionInfo> & sigs)
{
  QList<ActionInfo>::const_iterator it = sigs.begin();

  cf_assert( node.get() != nullptr );

  m_menu->clear();

  m_node = node;
  m_currentAction = nullptr;

  node->signal("signal_signature")->signal
      ->connect( boost::bind(&SignalManager::signalSignature, this, _1) );

//  connect(node->notifier(), SIGNAL(signalSignature(CF::Common::SignalArgs*)),
//          this, SLOT(signalSignature(CF::Common::SignalArgs*)));

  for( ; it!= sigs.end() ; it++)
  {
    if(!it->readableName.isEmpty())
    {
      QAction * action = m_menu->addAction(it->readableName);

      action->setStatusTip(it->description);

      connect(action, SIGNAL(triggered()), this, SLOT(actionTriggered()));
      connect(action, SIGNAL(hovered()), this, SLOT(actionHovered()));

      m_signals[action] = *it;
      m_localStatus[action] = it->isLocal;
    }
  }

  if(!m_menu->isEmpty())
    m_menu->exec(pos);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void SignalManager::actionTriggered()
{
  QAction * action = static_cast<QAction*>(sender());

  if(action != nullptr)
  {
    m_currentAction = action;
    m_waitingForSignature = true;

    if(!m_localStatus[action])
      m_node->requestSignalSignature( m_signals[action].name );
    else
    {
      SignalFrame frame("","","");
      m_node->localSignature(m_signals[action].name, frame);
      signalSignature(frame);
    }
  }
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void SignalManager::actionHovered()
{
  QAction * action = static_cast<QAction*>(sender());

  if(action != nullptr)
  {
    static_cast<QMainWindow*>(parent())->statusBar()->showMessage(action->statusTip(), 3000);
  }
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void SignalManager::signalSignature(SignalArgs & args)
{
  if(m_waitingForSignature)
  {
    URI path = m_node->full_path();
    ActionInfo & info = m_signals[m_currentAction];
    const char * tag = Protocol::Tags::key_options();

    m_frame = SignalFrame(info.name.toStdString(), path, path);
    SignalFrame& options = m_frame.map( Protocol::Tags::key_options() );

    if( args.has_map(tag) )
      args.map(tag).main_map.content.deep_copy( options.main_map.content );

    try
    {
      SignatureDialog * sg = new SignatureDialog();

      connect(sg, SIGNAL(finished(int)), this, SLOT(dialogFinished(int)));

      sg->show(options.main_map.content, m_currentAction->text());
    }
    catch( Exception & e)
    {
      NLog::globalLog()->addException(e.what());
    }
    catch( ... )
    {
      NLog::globalLog()->addException("Unknown exception caught");
    }



    m_waitingForSignature = false;
  }

}

////////////////////////////////////////////////////////////////////////////

void SignalManager::dialogFinished(int result)
{
  if(result == QDialog::Accepted)
  {
    SignatureDialog * dlg = static_cast<SignatureDialog*>(sender());

    if(dlg != nullptr)
    {
      if(m_localStatus[m_currentAction]) // if it is a local signal, call it...
      {
        try
        {
          m_node->call_signal(m_signals[m_currentAction].name.toStdString(), m_frame);
        }
        catch(InvalidURI ip)
        {
          NLog::globalLog()->addException(ip.what());
        }
      }
      else // ...or send the request to the server
        ThreadManager::instance().network().send(m_frame);

    }

    delete dlg;
  }
}

////////////////////////////////////////////////////////////////////////////

} // ClientUI
} // GUI
} // CF
