
/*****************************************************************************

  The following code is derived, directly or indirectly, from the SystemC
  source code Copyright (c) 1996-2011 by all Contributors.
  All Rights reserved.

  The contents of this file are subject to the restrictions and limitations
  set forth in the SystemC Open Source License Version 3.0 (the "License");
  You may not use this file except in compliance with such restrictions and
  limitations. You may obtain instructions on how to receive a copy of the
  License at http://www.systemc.org/. Software distributed by Contributors
  under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
  ANY KIND, either express or implied. See the License for the specific
  language governing rights and limitations under the License.

 *****************************************************************************/

/*****************************************************************************

  sc_export.cpp -- 

  Original Author: Bishnupriya Bhattachary, Cadence, Design Systems, 
                   25 August, 2003

 *****************************************************************************/

/*****************************************************************************

  MODIFICATION LOG - modifiers, enter your name, affiliation, date and
  changes you are making here.

      Name, Affiliation, Date:
  Description of Modification:
    
 *****************************************************************************/

// $Log: sc_export.cpp,v $
// Revision 1.5  2011/02/18 20:31:05  acg
//  Philipp A. Hartmann: added error messages for calls that cannot be done
//  after elaboration.
//
// Revision 1.4  2011/02/18 20:23:45  acg
//  Andy Goodrich: Copyright update.
//
// Revision 1.3  2011/02/18 20:07:04  acg
//  Philipp A. Hartmann: Patch to revert to sprintf from snprintf to keep
//  some versions of MSVC happy.
//
// Revision 1.2  2011/02/14 17:50:16  acg
//  Andy Goodrich: testing for sc_port and sc_export instantiations during
//  end of elaboration and issuing appropriate error messages.
//
// Revision 1.1.1.1  2006/12/15 20:20:04  acg
// SystemC 2.3
//
// Revision 1.4  2006/01/26 21:00:50  acg
//  Andy Goodrich: conversion to use sc_event::notify(SC_ZERO_TIME) instead of
//  sc_event::notify_delayed()
//
// Revision 1.3  2006/01/13 18:47:42  acg
// Added $Log command so that CVS comments are reproduced in the source.
//

#include "sysc/communication/sc_export.h"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_module.h"

namespace sc_core {

// ----------------------------------------------------------------------------
//  CLASS : sc_export_base
//
// ----------------------------------------------------------------------------

sc_export_base::sc_export_base() : sc_object(sc_gen_unique_name("export"))
{
    simcontext()->get_export_registry()->insert(this);
}
    
sc_export_base::sc_export_base(const char* name_) : sc_object(name_)
{
    simcontext()->get_export_registry()->insert(this);
}
    
sc_export_base::~sc_export_base()
{
    simcontext()->get_export_registry()->remove(this);
}


// called by construction_done (does nothing by default)

void
sc_export_base::before_end_of_elaboration()
{
}

// called when construction is done

void
sc_export_base::construction_done()
{
    if ( get_interface() == 0 )
    {
      report_error( SC_ID_SC_EXPORT_NOT_BOUND_AFTER_CONSTRUCTION_, 0);
    }
    sc_module* parent = DCAST<sc_module*>( get_parent_object() );
    sc_assert( parent );
    simcontext()->hierarchy_push( parent );
    before_end_of_elaboration();
    simcontext()->hierarchy_pop();
}

// called by elaboration_done (does nothing by default)

void
sc_export_base::end_of_elaboration()
{}

// called when elaboration is done

void
sc_export_base::elaboration_done()
{
    sc_module* parent = DCAST<sc_module*>( get_parent_object() );
    sc_assert( parent );
    simcontext()->hierarchy_push( parent );
    end_of_elaboration();
    simcontext()->hierarchy_pop();
}

// called by start_simulation (does nothing)

void
sc_export_base::start_of_simulation()
{}

// called before simulation starts

void
sc_export_base::start_simulation()
{
    sc_module* parent = DCAST<sc_module*>( get_parent_object() );
    sc_assert( parent );
    simcontext()->hierarchy_push( parent );
    start_of_simulation();
    simcontext()->hierarchy_pop();
}

// called by simulation_done (does nothing)

void
sc_export_base::end_of_simulation()
{}

// called after simulation ends

void
sc_export_base::simulation_done()
{
    sc_module* parent = DCAST<sc_module*>( get_parent_object() );
    sc_assert( parent );
    simcontext()->hierarchy_push( parent );
    end_of_simulation();
    simcontext()->hierarchy_pop();
}


void
sc_export_base::report_error( const char* id, const char* add_msg ) const
{
    char msg[BUFSIZ];
    if( add_msg != 0 ) {
        std::sprintf( msg, "%s: export '%s' (%s)", add_msg, name(), kind() );
    } else {
        std::sprintf( msg, "export '%s' (%s)", name(), kind() );
    }
    SC_REPORT_ERROR( id, msg );
}


// ----------------------------------------------------------------------------
//  CLASS : sc_export_registry
//
//  Registry for all exports.
//  FOR INTERNAL USE ONLY!
// ----------------------------------------------------------------------------

void
sc_export_registry::insert( sc_export_base* export_ )
{
    if( sc_is_running() ) {
	export_->report_error(SC_ID_INSERT_EXPORT_, "simulation running");
    }

    if( m_simc->elaboration_done()  ) {
       export_->report_error(SC_ID_INSERT_EXPORT_, "elaboration done");
    }


#ifdef DEBUG_SYSTEMC
    // check if port_ is already inserted
    for( int i = size() - 1; i >= 0; -- i ) {
	if( export_ == m_export_vec[i] ) {
	    export_->report_error( SC_ID_INSERT_EXPORT_, 
	                           "export already inserted ");
	}
    }
#endif

/* 
    //TBD:  maybe we want to do this stuf for later

    // append the port to the current module's vector of ports
    sc_module* curr_module = m_simc->hierarchy_curr();
    if( curr_module == 0 ) {
	port_->report_error( SC_ID_PORT_OUTSIDE_MODULE_ );
    }
    curr_module->append_port( port_ );
*/

    // insert
    m_export_vec.push_back( export_ );
}

void
sc_export_registry::remove( sc_export_base* export_ )
{
    if (size()==0) return;
    int i;
    for( i = size() - 1; i >= 0; -- i ) {
	if( export_ == m_export_vec[i] ) {
	    break;
	}
    }
    if( i == -1 ) {
	export_->report_error( SC_ID_SC_EXPORT_NOT_REGISTERED_ );
    }

    // remove
    m_export_vec[i] = m_export_vec[size() - 1];
    m_export_vec.resize(size()-1);
}

// constructor

sc_export_registry::sc_export_registry( sc_simcontext& simc_ )
  : m_construction_done(0)
  , m_simc( &simc_ )
{
}


// destructor

sc_export_registry::~sc_export_registry()
{
}

// called when construction is done

bool
sc_export_registry::construction_done()
{
    if( m_construction_done == size() )
      // nothing has been updated
      return true;

    for( int i = size()-1; i >= m_construction_done; --i ) {
        m_export_vec[i]->construction_done();
    }

    m_construction_done = size();
    return false;
}

// called when elaboration is done

void
sc_export_registry::elaboration_done()
{
    for( int i = size() - 1; i >= 0; -- i ) {
	m_export_vec[i]->elaboration_done();
    }
}

// called before simulation begins

void
sc_export_registry::start_simulation()
{
    for( int i = size() - 1; i >= 0; -- i ) {
	m_export_vec[i]->start_simulation();
    }
}

void
sc_export_registry::simulation_done()
{
    for( int i = size() - 1; i >= 0; -- i ) {
	m_export_vec[i]->simulation_done();
    }
}

} // namespace sc_core

// Taf!
