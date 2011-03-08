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

  sc_object.cpp -- Abstract base class of all SystemC objects.

  Original Author: Stan Y. Liao, Synopsys, Inc.

 *****************************************************************************/

/*****************************************************************************

  MODIFICATION LOG - modifiers, enter your name, affiliation, date and
  changes you are making here.

      Name, Affiliation, Date: Bishnupriya Bhattacharya, Cadence Design Systems,
                               25 August, 2003
  Description of Modification: if module name hierarchy is empty, sc_object 
                               ctor assumes the currently executing process 
                               as the parent object to support dynamic process
                               creation similar to other sc_objects

      Name, Affiliation, Date: Andy Goodrich, Forte Design Systems
                               5 September 2003
  Description of Modification: - Made creation of attributes structure
                                 conditional on its being used. This eliminates
                                 100 bytes of storage for each normal sc_object.

 *****************************************************************************/


// $Log: sc_object.cpp,v $
// Revision 1.12  2011/03/06 15:55:11  acg
//  Andy Goodrich: Changes for named events.
//
// Revision 1.11  2011/03/05 19:44:20  acg
//  Andy Goodrich: changes for object and event naming and structures.
//
// Revision 1.10  2011/03/05 04:45:16  acg
//  Andy Goodrich: moved active process calculation to the sc_simcontext class.
//
// Revision 1.9  2011/03/05 01:39:21  acg
//  Andy Goodrich: changes for named events.
//
// Revision 1.8  2011/02/18 20:27:14  acg
//  Andy Goodrich: Updated Copyrights.
//
// Revision 1.7  2011/02/13 21:47:37  acg
//  Andy Goodrich: update copyright notice.
//
// Revision 1.6  2011/01/25 20:50:37  acg
//  Andy Goodrich: changes for IEEE 1666 2011.
//
// Revision 1.5  2011/01/18 20:10:44  acg
//  Andy Goodrich: changes for IEEE1666_2011 semantics.
//
// Revision 1.4  2010/08/03 17:02:39  acg
//  Andy Goodrich: formatting changes.
//
// Revision 1.3  2009/02/28 00:26:58  acg
//  Andy Goodrich: changed boost name space to sc_boost to allow use with
//  full boost library applications.
//
// Revision 1.2  2008/05/22 17:06:26  acg
//  Andy Goodrich: updated copyright notice to include 2008.
//
// Revision 1.1.1.1  2006/12/15 20:20:05  acg
// SystemC 2.3
//
// Revision 1.5  2006/04/20 17:08:17  acg
//  Andy Goodrich: 3.0 style process changes.
//
// Revision 1.4  2006/03/21 00:00:34  acg
//   Andy Goodrich: changed name of sc_get_current_process_base() to be
//   sc_get_current_process_b() since its returning an sc_process_b instance.
//
// Revision 1.3  2006/01/13 18:44:30  acg
// Added $Log to record CVS changes into the source.
//

#include <stdio.h>
#include <cstdlib>
#include <cassert>
#include <ctype.h>

#include "sysc/kernel/sc_externs.h"
#include "sysc/kernel/sc_kernel_ids.h"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_object.h"
#include "sysc/kernel/sc_object_manager.h"
#include "sysc/kernel/sc_process_handle.h"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_event.h"
#include "sysc/utils/sc_hash.h"
#include "sysc/utils/sc_iostream.h"
#include "sysc/utils/sc_list.h"
#include "sysc/utils/sc_mempool.h"

namespace sc_core {

typedef int (*STRCMP)(const void*, const void*);

const char SC_HIERARCHY_CHAR = '.';

/* This will be gotten rid after multiple-processes
   are implemented.  This is to fix some regression
   problems. */
bool sc_enable_name_checking = true;


// ----------------------------------------------------------------------------
//  CLASS : sc_object
//
//  Abstract base class of all SystemC `simulation' objects.
// ----------------------------------------------------------------------------

void
sc_object::add_child_event( sc_event* event_p )
{
    // no check if event_p is already in the set
    m_child_events.push_back( event_p );
}

void
sc_object::add_child_object( sc_object* object_ )
{
    // no check if object_ is already in the set
    m_child_objects.push_back( object_ );
}

const char*
sc_object::basename() const
{
    size_t pos; // position of last SC_HIERARCHY_CHAR.
    pos = m_name.rfind( (char)SC_HIERARCHY_CHAR );
    return ( pos == m_name.npos ) ? m_name.c_str() : &(m_name.c_str()[pos+1]);
} 

void
sc_object::print(::std::ostream& os) const
{
    os << name();
}

void
sc_object::dump(::std::ostream& os) const
{
    os << "name = " << name() << "\n";
    os << "kind = " << kind() << "\n";
}

static int sc_object_num = 0;

static std::string
sc_object_newname()
{
    char        buffer[64];
    std::string result;

    std::sprintf(buffer, "{%d}", sc_object_num);
    sc_object_num++;
    result = buffer;

    return result;
}

// +----------------------------------------------------------------------------
// |"sc_object::remove_child_event"
// | 
// | This virtual method removes the supplied event from the list of child
// | events if it is present.
// |
// | Arguments:
// |     event_p -> event to be removed.
// | Returns true if the event was present, false if not.
// +----------------------------------------------------------------------------
bool
sc_object::remove_child_event( sc_event* event_p )
{
    int size = m_child_events.size();
    for( int i = 0; i < size; ++ i ) {
        if( event_p == m_child_events[i] ) {
            m_child_events[i] = m_child_events[size - 1];
            m_child_events.pop_back();
            return true;
        }
    }
    return false;
}

// +----------------------------------------------------------------------------
// |"sc_object::remove_child_object"
// | 
// | This virtual method removes the supplied object from the list of child
// | objects if it is present.
// |
// | Arguments:
// |     object_p -> object to be removed.
// | Returns true if the object was present, false if not.
// +----------------------------------------------------------------------------
bool
sc_object::remove_child_object( sc_object* object_p )
{
    int size = m_child_objects.size();
    for( int i = 0; i < size; ++ i ) {
        if( object_p == m_child_objects[i] ) {
            m_child_objects[i] = m_child_objects[size - 1];
            m_child_objects.pop_back();
	    object_p->m_parent = NULL;
            return true;
        }
    }
    return false;
}

// +----------------------------------------------------------------------------
// |"sc_object::sc_object_init"
// | 
// | This method initializes this object instance and places it in to the
// | object hierarchy if the supplied name is not NULL.
// |
// | Arguments:
// |     nm = leaf name for the object.
// +----------------------------------------------------------------------------
void 
sc_object::sc_object_init(const char* nm) 
{ 
    // @@@@#### REMOVE bool        clash;                  // true if path name exists in obj table
    // @@@@#### REMOVE const char* leafname_p;             // leaf name (this object) 
    // @@@@#### REMOVE char        pathname[BUFSIZ];       // path name 
    // @@@@#### REMOVE char        pathname_orig[BUFSIZ];  // original path name which may clash 
    // @@@@#### REMOVE const char* parentname_p;           // parent path name 
    // @@@@#### sc_object*  parent_p;               // parent for this instance or NULL.
    // @@@@#### REMOVE bool        put_in_table;           // true if should put in object table 
 
    // SET UP POINTERS TO OBJECT MANAGER, PARENT, AND SIMULATION CONTEXT: 
    //
    // Make the current simcontext the simcontext for this object 

    m_simc = sc_get_curr_simcontext(); 
    m_attr_cltn_p = 0; 
    sc_object_manager* object_manager = m_simc->get_object_manager(); 
    m_parent = m_simc->active_object();
    // @@@@#### REMOVE m_parent = parent_p; 


    // CONSTRUCT PATHNAME TO OBJECT BEING CREATED: 
    // 
    // If there is not a leaf name generate one. 

#if 0 // @@@@#### REMOVE
    parentname_p = parent_p ? parent_p->name() : ""; 
    if (nm ) 
    { 
        leafname_p = nm; 
        put_in_table = true; 
    } 
    else 
    { 
        leafname_p = sc_object_newname().c_str();
        put_in_table = false; 
    } 
    if (parent_p) { 
        std::sprintf(pathname, "%s%c%s", parentname_p, 
                SC_HIERARCHY_CHAR, leafname_p 
        ); 
    } else { 
        strcpy(pathname, leafname_p); 
    } 

    // SAVE the original path name 
    // 
    strcpy(pathname_orig, pathname); 

    // MAKE SURE THE OBJECT NAME IS UNIQUE 
    // 
    // If not use unique name generator to make it unique. 

    clash = false; 
    while (object_manager->find_object(pathname)) { 
        clash = true; 
        leafname_p = sc_gen_unique_name(leafname_p); 
        if (parent_p) { 
            std::sprintf(pathname, "%s%c%s", parentname_p, 
                    SC_HIERARCHY_CHAR, leafname_p 
            ); 
        } else { 
            strcpy(pathname, leafname_p); 
        } 
    } 
    if (clash) { 
	std::string message = pathname_orig;
	message += ". Latter declaration will be renamed to ";
	message += pathname;
        SC_REPORT_WARNING( SC_ID_INSTANCE_EXISTS_, message.c_str());
    } 

    m_name = pathname;
#else
    m_name = object_manager->create_name(nm ? nm : sc_object_newname().c_str());
#endif


    // PLACE THE OBJECT INTO THE HIERARCHY IF A LEAF NAME WAS SUPPLIED:
    

    if (nm != NULL) { 
        object_manager->insert_object(m_name, this); 
        if ( m_parent ) 
	    m_parent->add_child_object( this );
	else
	    m_simc->add_child_object( this ); 
    } 

} 

sc_object::sc_object() : m_parent(0)
{
    sc_object_init( sc_gen_unique_name("object") );
}

sc_object::sc_object( const sc_object& that ) : m_parent(0)
{
    sc_object_init( sc_gen_unique_name( that.basename() ) );
}


static bool
object_name_illegal_char(char ch)
{
    return (ch == SC_HIERARCHY_CHAR) || isspace(ch);
}

sc_object::sc_object(const char* nm) : m_parent(0)
{
    int namebuf_alloc = 0;
    char* namebuf = 0;
    const char* p;

	// null name or "" uses machine generated name.
    if ( !nm || strlen(nm) == 0 )
	nm = sc_gen_unique_name("object");
    p = nm;

    if (nm && sc_enable_name_checking) {
        namebuf_alloc = 1 + strlen(nm);
        namebuf = (char*) sc_mempool::allocate(namebuf_alloc);
        char* q = namebuf;
        const char* r = nm;
        bool has_illegal_char = false;
        while (*r) {
            if (object_name_illegal_char(*r)) {
                has_illegal_char = true;
                *q = '_';
            } else {
                *q = *r;
            }
            r++;
            q++;
        }
        *q = '\0';
        p = namebuf;
        if (has_illegal_char)
	{
	    std::string message = nm;
	    message += " substituted by ";
	    message += namebuf;
            SC_REPORT_WARNING( SC_ID_ILLEGAL_CHARACTERS_, message.c_str());
	}
    }
    sc_object_init(p);
    sc_mempool::release( namebuf, namebuf_alloc );
}

sc_object::~sc_object()
{
    detach();
    if ( m_attr_cltn_p ) delete m_attr_cltn_p;
}

//------------------------------------------------------------------------------
//"sc_object::detach"
//
// This method detaches this object instance from the object hierarchy.
// It is called in two places: ~sc_object() and sc_process_b::kill_process().
//------------------------------------------------------------------------------
void sc_object::detach()
{
    if (m_simc) {

        // REMOVE OBJECT FROM THE OBJECT MANAGER:

        sc_object_manager* object_manager = m_simc->get_object_manager();
        object_manager->remove_object(m_name);

		// REMOVE OBJECT FROM PARENT'S LIST OF OBJECTS:

        if ( m_parent )
	    m_parent->remove_child_object( this );
	else
	    m_simc->remove_child_object( this );

        // ORPHAN THIS OBJECT'S CHILDREN:

#if 0 // ####
	    ::std::<sc_object*> children_p = &get_child_objects();
		int                 child_n = children_p->size();
		sc_object*          parent_p;

		for ( int child_i = 0; child_i < child_n; child_i++ )
		{
			(*children_p)[child_i]->m_parent = 0;
		}
#endif

    }
}

// +----------------------------------------------------------------------------
// |"sc_object::orphan_child_events"
// | 
// | This method moves the children of this object instance to be children
// | of the simulator.
// +----------------------------------------------------------------------------
void sc_object::orphan_child_events()
{
    std::vector< sc_event* > const & events = get_child_events();

    std::vector< sc_event* >::const_iterator
            it  = events.begin(), end = events.end();

    for( ; it != end; ++it  )
    {
        (*it)->m_parent_p = NULL;
        simcontext()->add_child_event(*it);
    }
}

// +----------------------------------------------------------------------------
// |"sc_object::orphan_child_objects"
// | 
// | This method moves the children of this object instance to be children
// | of the simulator.
// +----------------------------------------------------------------------------
void sc_object::orphan_child_objects()
{
    std::vector< sc_object* > const & children = get_child_objects();

    std::vector< sc_object* >::const_iterator
            it  = children.begin(), end = children.end();

    for( ; it != end; ++it  )
    {
        (*it)->m_parent = NULL;
        simcontext()->add_child_object(*it);
    }
}

void
sc_object::trace( sc_trace_file * /* unused */) const
{
    /* This space is intentionally left blank */
}


// add attribute

bool
sc_object::add_attribute( sc_attr_base& attribute_ )
{
    if ( !m_attr_cltn_p ) m_attr_cltn_p = new sc_attr_cltn;
    return ( m_attr_cltn_p->push_back( &attribute_ ) );
}


// get attribute by name

sc_attr_base*
sc_object::get_attribute( const std::string& name_ )
{
    if ( !m_attr_cltn_p ) m_attr_cltn_p = new sc_attr_cltn;
    return ( (*m_attr_cltn_p)[name_] );
}

const sc_attr_base*
sc_object::get_attribute( const std::string& name_ ) const
{
    if ( !m_attr_cltn_p ) m_attr_cltn_p = new sc_attr_cltn;
    return ( (*m_attr_cltn_p)[name_] );
}


// remove attribute by name

sc_attr_base*
sc_object::remove_attribute( const std::string& name_ )
{
    if ( m_attr_cltn_p )
	return ( m_attr_cltn_p->remove( name_ ) );
    else
	return 0;
}


// remove all attributes

void
sc_object::remove_all_attributes()
{
    if ( m_attr_cltn_p )
	m_attr_cltn_p->remove_all();
}


// get the number of attributes

int
sc_object::num_attributes() const
{
    if ( m_attr_cltn_p )
	return ( m_attr_cltn_p->size() );
    else
	return 0;
}


// get the attribute collection

sc_attr_cltn&
sc_object::attr_cltn()
{
    if ( !m_attr_cltn_p ) m_attr_cltn_p = new sc_attr_cltn;
    return *m_attr_cltn_p;
}

const sc_attr_cltn&
sc_object::attr_cltn() const
{
    if ( !m_attr_cltn_p ) m_attr_cltn_p = new sc_attr_cltn;
    return *m_attr_cltn_p;
}

} // namespace sc_core

