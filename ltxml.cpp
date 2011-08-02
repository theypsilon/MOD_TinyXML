#include "tinyxml.h"
//#include "xpath_processor.h"
#include <string>
/*
#include <iostream>
#include <sstream>
*/
using namespace std;

extern "C" {
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"
}

static int DOCUMENTS_INDEX(LUA_NOREF);
static int DOCUMENTS_REFCOUNT_INDEX(LUA_NOREF);

typedef struct TiXmlDocument_ud {
  TiXmlDocument *xmldoc;
  char* filename;
} TiXmlDocument_ud;


typedef struct TiXmlNode_ud {
  TiXmlNode *xmlnode;
} TiXmlNode_ud;


/*int lua_stackdump(lua_State *L) {
  int i;
  int top = lua_gettop(L);
  std::string sd = "stack dump: ";

  for (i = 1; i <= top; i++) {
    int t = lua_type(L, i);
    switch (t) {
    case LUA_TSTRING:
      sd += lua_pushfstring(L, "`%s'", lua_tostring(L, i));
      lua_pop(L, 1);
      break;
    case LUA_TBOOLEAN:
      sd += lua_toboolean(L, i) ? "true" : "false";
      break;
    case LUA_TNUMBER:
      sd += lua_pushfstring(L, "%f", lua_tonumber(L, i));
      lua_pop(L, 1);
      break;
    default:
      sd += lua_typename(L, t);
      break;
    }
    sd += "\n            ";
  }

  cout << sd << endl;
  //send_log("DEBUG", sd.c_str(), "INTERNAL");

  return 0;
}
*/


void decrease_tixmldocument_refcount(lua_State *L, const TiXmlDocument *xmldoc) {
  long docptr(reinterpret_cast<long>(xmldoc));

  lua_rawgeti(L, LUA_REGISTRYINDEX, DOCUMENTS_REFCOUNT_INDEX);
  int refcount_table_index(lua_gettop(L));
  lua_pushinteger(L, docptr);
  lua_gettable(L, -2);

  if (lua_istable(L, -1)) {
    lua_pushinteger(L, 2);
    lua_gettable(L, -2);

    int refcount(lua_tointeger(L, -1));
    lua_pop(L, 1);

    if (refcount == 1) {
      // destroy table
      lua_pushinteger(L, docptr);
      lua_pushnil(L);
      lua_settable(L, refcount_table_index);
      //cout << "current refcount for " << xmldoc << " : 0" << endl;
    } else {
      lua_pushinteger(L, 2);
      lua_pushinteger(L, refcount-1);
      lua_settable(L, -3);
      //cout << "current refcount for " << xmldoc << " : " << refcount-1 << endl;
    }
  }

  lua_pop(L, 2);
}

void increase_tixmldocument_refcount(lua_State *L, const TiXmlDocument* xmldoc) {
  // increase ref. count for corresponding document in DOCUMENTS table
  long docptr(reinterpret_cast<long>(xmldoc));

  // 1) get document's userdata
  lua_rawgeti(L, LUA_REGISTRYINDEX, DOCUMENTS_INDEX);
  lua_pushinteger(L, docptr);
  lua_gettable(L, -2);
  TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) lua_touserdata(L, -1);
  int ud_index(lua_gettop(L));

  if (xmldoc_userdata) {
    // 2) get refcount table
    lua_rawgeti(L, LUA_REGISTRYINDEX, DOCUMENTS_REFCOUNT_INDEX);
    lua_pushinteger(L, docptr);
    lua_gettable(L, -2);

    if (lua_istable(L, -1)) {
      // 3) increase refcount ...
      lua_pushinteger(L, 2);
      lua_gettable(L, -2);
      int refcount(lua_tointeger(L, -1));
      lua_pop(L, 1);

      lua_pushinteger(L, 2);
      lua_pushinteger(L, refcount + 1);
      lua_settable(L, -3);

      //cout << "current refcount for " << xmldoc << " : " << refcount+1 << endl;
    } else {
      // ... or create new refcount table
      lua_pushinteger(L, docptr);

      lua_createtable(L, 2, 0);
      lua_pushinteger(L, 1);
      lua_pushvalue(L, ud_index); //create a strong reference to the document's ud
      lua_settable(L, -3);
      lua_pushinteger(L, 2);
      lua_pushinteger(L, 1);
      lua_settable(L, -3);

      lua_settable(L, ud_index+1);

      //cout << "current refcount for " << xmldoc << " : 1" << endl;
    }

    lua_pop(L, 2);
  }

  lua_pop(L, 2);
}


TiXmlNode* find_node(const char *path, TiXmlNode *start_node, char** attribute=NULL)
  {
    int index(0);
    std::string str(path), name;
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of("/", 0);
    // Find first "non-delimiter".
    string::size_type pos = str.find_first_of("/", lastPos);

    if (attribute)
      *attribute = NULL;

    TiXmlNode* node = NULL;
    while (string::npos != pos || string::npos != lastPos) {
      // Found a token
      name = str.substr(lastPos, pos - lastPos);

      if (name[0]=='@') {
	// check attribute name
	if (attribute) {
	  std::string attr_name = name.substr(1);
	  (*attribute) = new char[attr_name.length()];
	  strcpy(*attribute, attr_name.c_str());
	}
	break;
      } else {
	index=0;
	string::size_type i = name.rfind("[");
	if (i != string::npos) {
	  index = atoi(name.substr(i+1, name.length()-2-i).c_str());
	  name = name.substr(0, i);
	}
      }

      if (node)
	node=node->FirstChild();
      else
	node=start_node;

      int i=0;
      while (node) {
	if (node->Type() == TiXmlNode::ELEMENT) {
	  //cout << "comparing " << name << " and " << start_node->Value() << std::endl;
	  if (strcmp(name.c_str(), node->Value()) == 0) {
	    i++;
	    if ((index<=0)||(i==index))
	      goto FIND_NEXT;
	  }
	}

	node = node->NextSibling();
      }

      return NULL;

    FIND_NEXT:
      // Skip delimiters.  Note the "not_of"
      lastPos = str.find_first_not_of("/", pos);
      // Find next "non-delimiter"
      pos = str.find_first_of("/", lastPos);
    }

    return node;
  }


extern "C" {
  int tixmlnode_repr(lua_State *L) {
    TiXmlNode *xmlnode;
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");
    xmlnode = xmlnode_userdata->xmlnode;

    lua_pop(L, 1);

    if (xmlnode) {
      lua_pushfstring(L, "< %s node>", xmlnode->Value());
    } else {
      return luaL_error(L, "invalid node");
    }

    return 1;
  }


  int tixmlnode_close(lua_State *L) {
    TiXmlNode *xmlnode;
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");
    xmlnode = xmlnode_userdata->xmlnode;

    lua_pop(L, 1);

    // decrease refcount for corresponding document
    if (xmlnode)
      decrease_tixmldocument_refcount(L, xmlnode->GetDocument());

    xmlnode_userdata->xmlnode = NULL;

    return 0;
  }


  int l_xmlopen(lua_State* L) {
    const char *filename = luaL_checkstring(L, 1);
    TiXmlDocument_ud *xpu;

    lua_pop(L, 1);

    xpu = (TiXmlDocument_ud *) lua_newuserdata(L, sizeof(TiXmlDocument_ud));
    // set metatable for userdata
    luaL_getmetatable(L, "TiXmlDocument");
    lua_setmetatable(L, -2);

    TiXmlDocument *xmldoc = new TiXmlDocument(filename);
    xpu->filename = (char *) malloc(strlen(filename)+1);
    strcpy(xpu->filename, filename);
    xpu->xmldoc = xmldoc;
    bool loadOkay = xmldoc->LoadFile();

    if ( !loadOkay )
      return luaL_error(L, "error: could not load file %s: %s", filename, xmldoc->ErrorDesc());

    // put document into DOCUMENTS_INDEX table
    lua_rawgeti(L, LUA_REGISTRYINDEX, DOCUMENTS_INDEX);
    lua_pushinteger(L, reinterpret_cast<long>(xmldoc));
    lua_pushvalue(L, -3);
    lua_settable(L, -3);
    lua_pop(L, 1);

    return 1;
  }


  int l_xmlparse(lua_State* L) {
    const char* xml = luaL_checkstring(L, 1);
    const char* filename = "string";
    TiXmlDocument_ud *xpu;

    lua_pop(L, 1);

    xpu = (TiXmlDocument_ud *) lua_newuserdata(L, sizeof(TiXmlDocument_ud));
    // set metatable for userdata
    luaL_getmetatable(L, "TiXmlDocument");
    lua_setmetatable(L, -2);

    TiXmlDocument *xmldoc = new TiXmlDocument();
    xpu->filename = (char *) malloc(strlen(filename)+1);
    strcpy(xpu->filename, filename);
    xpu->xmldoc = xmldoc;
    xmldoc->Parse(xml);

    if (xmldoc->Error())
      return luaL_error(L, "could not parse xml string: %s", xmldoc->ErrorDesc());

    // put document into DOCUMENTS_INDEX table
    lua_rawgeti(L, LUA_REGISTRYINDEX, DOCUMENTS_INDEX);
    lua_pushinteger(L, reinterpret_cast<long>(xmldoc));
    lua_pushvalue(L, -3);
    lua_settable(L, -3);
    lua_pop(L, 1);

    return 1;
  }


  int tixmldocument_repr(lua_State *L) {
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");

    lua_pop(L, 1);

    lua_pushfstring(L, "<xml document: '%s`>", xmldoc_userdata->filename);

    return 1;
  }


  int tixmldocument_getroot(lua_State *L) {
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");
    TiXmlDocument *xmldoc = xmldoc_userdata->xmldoc;

    lua_pop(L, 1);

    TiXmlNode_ud* node_userdata = (TiXmlNode_ud *) lua_newuserdata(L, sizeof(TiXmlNode_ud));
    node_userdata->xmlnode = xmldoc->RootElement(); //->Clone();

    luaL_newmetatable(L, "TiXmlNode");
    lua_setmetatable(L, -2);

    increase_tixmldocument_refcount(L, xmldoc);

    return 1;
  }


  int tixmldocument_write(lua_State *L) {
    TiXmlDocument *xmldoc;
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");
    xmldoc = xmldoc_userdata->xmldoc;
    const char *path = luaL_checkstring(L, 2); // path
    const char *value = luaL_checkstring(L, 3); // new value

    lua_pop(L, 3);

    char *attr_name;
    TiXmlNode *node = find_node(path, xmldoc->RootElement(), &attr_name);

    if (node) {
      if (attr_name) {
	if (node->Type() == TiXmlNode::ELEMENT) {
	  TiXmlElement *elt_node = dynamic_cast<TiXmlElement *>(node);

	  if (elt_node->Attribute(attr_name)) {
	    elt_node->SetAttribute(attr_name, value);
	    delete attr_name;
	    lua_pushboolean(L, 1);
	    return 1;
	  }
	}
	luaL_error(L, "invalid attribute: %s", attr_name);
	delete attr_name;
	return 0;
      } else {
	TiXmlNode *n = node->FirstChild();
	if (n) {
	  if (n->Type()==TiXmlNode::TEXT) {
	    n->SetValue(value);
	  } else {
	    return luaL_error(L, "%s does not point to a text node", path);
	  }
	} else {
	  // create the text child
	  TiXmlText *new_text_node = new TiXmlText(value);
	  // and add it
	  node->LinkEndChild(new_text_node);
	}
      }

      lua_pushboolean(L, 1);
      return 1;
    } else {
      return luaL_error(L, "path not found: %s", path);
    }
  }


  int tixmldocument_save(lua_State *L) {
    TiXmlDocument *xmldoc;
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");
    xmldoc = xmldoc_userdata->xmldoc;

    lua_pop(L, 1);

    if (xmldoc->SaveFile()) {
      lua_pushboolean(L, 1);
      return 1;
    } else {
      return luaL_error(L, "could not save xml file");
    }
  }


  int tixmldocument_delete(lua_State *L) {
    TiXmlDocument *xmldoc;
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");
    xmldoc = xmldoc_userdata->xmldoc;
    const char *path = luaL_checkstring(L, 2); // path

    lua_pop(L, 2);

    char *attr_name;
    TiXmlNode *node = find_node(path, xmldoc->RootElement(), &attr_name);

    if (node) {
      if (attr_name) {
	if (node->Type() == TiXmlNode::ELEMENT) {
	  TiXmlElement *elt_node = dynamic_cast<TiXmlElement *>(node);
	  if (elt_node->Attribute(attr_name)) {
	    elt_node->RemoveAttribute(attr_name);
	    delete attr_name;
	    lua_pushboolean(L, 1);
	    return 1;
	  }
	}
	luaL_error(L, "invalid attribute: %s", attr_name);
	delete attr_name;
	return 0;
      } else {
	node->Parent()->RemoveChild(node);
	lua_pushboolean(L, 1);
	return 1;
      }
    } else {
      return luaL_error(L, "path not found: %s", path);
    }
  }


  int tixmldocument_add(lua_State *L) {
    TiXmlDocument *xmldoc;
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");
    xmldoc = xmldoc_userdata->xmldoc;
    const char *parent_path = luaL_checkstring(L, 2);
    const char *xml = luaL_checkstring(L, 3);

    lua_pop(L, 3);

    char *attr_name;
    TiXmlNode *parent_node = find_node(parent_path, xmldoc->RootElement(), &attr_name);

    if (parent_node) {
      if (attr_name) {
	// just add new attribute
	delete attr_name;
	return 0;
      } else {
	// try to parse xml, and add it to node
	TiXmlDocument d;
	d.Parse(xml);

	TiXmlNode *new_node = d.RootElement();

	if (new_node) {
	  parent_node->LinkEndChild(new_node->Clone());
	  lua_pushboolean(L, 1);
	  return 1;
	}

	return luaL_error(L, "could not add new child node");
      }
    } else {
      return luaL_error(L, "path not found: %s", parent_path);
    }
  }

/*
  int tixmldocument_print(lua_State *L) {
    TiXmlDocument *xmldoc;
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");
    xmldoc = xmldoc_userdata->xmldoc;

    lua_pop(L, 1);

    TiXmlPrinter p;
    xmldoc->Accept(&p);

    lua_pushstring(L, p.CStr());

    return 1;
  }
*/

  int tixmldocument_close(lua_State *L) {
    TiXmlDocument *xmldoc;
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");
    xmldoc = xmldoc_userdata->xmldoc;

    lua_pop(L, 1);

    // free TiXmlDocument instance if there is one
    if (xmldoc) {
      delete xmldoc;
      xmldoc_userdata->xmldoc = NULL;
    }

    free(xmldoc_userdata->filename);
    xmldoc_userdata->filename = NULL;

    //cout << "DOCUMENT HAS BEEN COLLECTED" << endl;

    return 0;
  }


  int l_select(lua_State *L) {
    const char *expr = luaL_checkstring(L, 1);
    TiXmlNode *node = (TiXmlNode *) lua_touserdata(L, 2);

    TinyXPath::xpath_processor xpp(node, expr);
    TinyXPath::expression_result res(xpp.er_compute_xpath());

    if (xpp.e_error != TinyXPath::xpath_processor::e_no_error) {
      string errmsg("error while computing xpath query `");
      errmsg += expr;
      errmsg += "'";
      return luaL_error(L, errmsg.c_str());
    }

    switch (res.e_type) {
    case TinyXPath::e_string:
      lua_pushstring(L, res.cp_get_string());
      return 1;
    case TinyXPath::e_node_set:
      {
	TinyXPath::node_set* ns;
	const TiXmlNode* node;
	ns = res.nsp_get_node_set();
	int nns(ns->u_get_nb_node_in_set());

	if (nns == 0) {
	  lua_pushnil(L);
	  return 1;
	}

	lua_createtable(L, nns, 0);

	for (int i=0; i<nns; i++) {
	  lua_pushinteger(L, i+1);

	  if (ns->o_is_attrib(i)) {
	    std::string attr_value = ns->S_get_value(i);
	    lua_pushstring(L, attr_value.c_str());
	  } else {
	    node = ns->XNp_get_node_in_set(i);

	    if (node->Type() == TiXmlNode::TEXT) {
	      lua_pushstring(L, node->Value());
	      break;
	    } else {
	      TiXmlNode_ud* node_userdata = (TiXmlNode_ud *) lua_newuserdata(L, sizeof(TiXmlNode_ud));
	      node_userdata->xmlnode = const_cast<TiXmlNode *>(node); //->Clone();

              luaL_newmetatable(L, "TiXmlNode");
	      lua_setmetatable(L, -2);

	      increase_tixmldocument_refcount(L, node->GetDocument());
	    }
	  }

	  lua_settable(L, -3);
	}

	return 1;
      }
    case TinyXPath::e_bool:
      lua_pushboolean(L, res.o_get_bool());
      return 1;
    case TinyXPath::e_int:
      lua_pushinteger(L, res.i_get_int());
      return 1;
    case TinyXPath::e_double:
      lua_pushnumber(L, res.d_get_double());
      return 1;
    case TinyXPath::e_invalid:
      return luaL_error(L, "invalid xpath expression");
    default:
      lua_pushnil(L);
      return 1;
    }
  }


  // given a xpath string, returns the evaluation
  // of the expression on the tree
  int tixmldocument_select(lua_State *L) {
    TiXmlDocument_ud* xmldoc_userdata = (TiXmlDocument_ud *) luaL_checkudata(L, 1, "TiXmlDocument");
    luaL_checkstring(L, 2);

    lua_remove(L, 1);
    lua_pushlightuserdata(L, xmldoc_userdata->xmldoc->RootElement());

    return l_select(L);
  }


  int tixmlnode_select(lua_State *L) {
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");
    luaL_checkstring(L, 2);

    lua_remove(L, 1);
    lua_pushlightuserdata(L, xmlnode_userdata->xmlnode);

    return l_select(L);
  }


  int tixmlnode_print(lua_State *L) {
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");
    const TiXmlNode* node = xmlnode_userdata->xmlnode;

    lua_pop(L, 1);

    TiXmlPrinter p;
    node->Accept(&p);

    lua_pushstring(L, p.CStr());
    //lua_pushinteger(L, node->GetDocument());

    return 1; //2;
  }


  int tixmlnode_text(lua_State *L) {
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");
    const TiXmlNode* node = xmlnode_userdata->xmlnode;

    if (lua_gettop(L)>1) {
      const char *child_name=lua_tostring(L, 2);
      lua_settop(L, 0);

      if (node->Type() == TiXmlNode::ELEMENT) {
	const TiXmlNode *child = node->FirstChild();

	while (child) {
	  if ((child->Type() == TiXmlNode::ELEMENT)&&(strcmp(child->Value(), child_name)==0)) {
	    const TiXmlElement *elt = dynamic_cast<const TiXmlElement *>(child);
	    const char *text = elt->GetText();
	    if (text) {
	      lua_pushstring(L, text);
	    } else {
	      lua_pushstring(L, "");
	    }
	    return 1;
	  }
	  child = child->NextSibling();
	}
      }

      lua_pushnil(L);
      return 1;
    }

    lua_settop(L, 0);

    if (node->Type() == TiXmlNode::ELEMENT) {
      const TiXmlElement *elt = dynamic_cast<const TiXmlElement *>(node);
      const char *text = elt->GetText();
      if (text) {
	lua_pushstring(L, text);
      } else {
	lua_pushstring(L, "");
      }
    } else {
      lua_pushstring(L, node->Value());
    }

    return 1;
  }


  int tixmlnode_name(lua_State *L) {
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");

    lua_pop(L, 1);

    if (xmlnode_userdata->xmlnode->Type() == TiXmlNode::ELEMENT) {
      lua_pushstring(L, xmlnode_userdata->xmlnode->Value());
    } else {
      lua_pushnil(L);
    }

    return 1;
  }


  int tixmlnode_expand(lua_State *L) {
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");
    const TiXmlNode* node = xmlnode_userdata->xmlnode;
    lua_pop(L, 1);

    lua_newtable(L);
    if (node->NoChildren()) {
      // no child => just add name: value pair to the table,
      // if node is an element (otherwise don't do anything)
      if (node->Type() == TiXmlNode::ELEMENT) {
	const TiXmlElement *elt = dynamic_cast<const TiXmlElement *>(node);
	lua_pushstring(L, elt->Value());
	lua_pushstring(L, elt->GetText());
	lua_settable(L, -3);
      }
    } else {
      const TiXmlNode *child = node->FirstChild();

      while (child) {
	// go through all element children and if they
	// are leafs, add name:value to table otherwise
	// add a reference to the node object
	if (child->Type() == TiXmlNode::ELEMENT) {
	  if ((child->NoChildren())||(child->FirstChild()->Type()==TiXmlNode::TEXT)) {
	    const TiXmlElement *elt = dynamic_cast<const TiXmlElement *>(child);
	    lua_pushstring(L, elt->Value());
	    lua_pushstring(L, elt->GetText());

	    lua_settable(L, -3);
	  }
	}

	child = child->NextSibling();
      }
    }

    return 1;
  }


  int tixmlnode_child(lua_State *L) {
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");

    lua_pop(L, 1);

    TiXmlNode *first_child = xmlnode_userdata->xmlnode->FirstChild();

    if (first_child) {
      if (first_child->Type() == TiXmlNode::ELEMENT) {
	TiXmlNode_ud* node_userdata = (TiXmlNode_ud *) lua_newuserdata(L, sizeof(TiXmlNode_ud));
	node_userdata->xmlnode = first_child; //->Clone();

	luaL_newmetatable(L, "TiXmlNode");
	lua_setmetatable(L, -2);

	increase_tixmldocument_refcount(L, first_child->GetDocument());
      } else {
	lua_pushnil(L);
	return 1;
      }
    } else {
      lua_pushnil(L);
    }

    return 1;
  }


  int tixmlnode_nextsibling(lua_State *L) {
    TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");

    lua_pop(L, 1);

    TiXmlNode *next_sibling = xmlnode_userdata->xmlnode->NextSibling();

    if (next_sibling) {
      if (next_sibling->Type() == TiXmlNode::ELEMENT) {
	TiXmlNode_ud* node_userdata = (TiXmlNode_ud *) lua_newuserdata(L, sizeof(TiXmlNode_ud));
	node_userdata->xmlnode = next_sibling; //->Clone();

	luaL_newmetatable(L, "TiXmlNode");
	lua_setmetatable(L, -2);

	increase_tixmldocument_refcount(L, next_sibling->GetDocument());
      } else {
	lua_pushnil(L);
	return 1;
      }
    } else {
      lua_pushnil(L);
    }

    return 1;
  }


  int tixmlnode_getattr(lua_State *L) {
     TiXmlNode_ud* xmlnode_userdata = (TiXmlNode_ud *) luaL_checkudata(L, 1, "TiXmlNode");
     const char *attr_name = luaL_checkstring(L, 2);

     lua_pop(L, 2);

     if (xmlnode_userdata->xmlnode->Type()==TiXmlNode::ELEMENT) {
       TiXmlElement *elt = dynamic_cast<TiXmlElement *>(xmlnode_userdata->xmlnode);
       const char *attr_val = elt->Attribute(attr_name);

       if (attr_val) {
	 lua_pushstring(L, attr_val);
	 return 1;
       } else {
	 lua_pushnil(L);
	 return 1;
       }
     } else {
       return luaL_error(L, "attribute() called on a non-element node");
     }
  }


  static const struct luaL_reg TinyXML[] = {
    { "open", l_xmlopen},
    { "parse", l_xmlparse},
    {NULL, NULL} /* sentinel */
  };

  static const struct luaL_reg TiXmlDocument_methods[] = {
    { "print",  tixmldocument_print },
    { "__gc", tixmldocument_close },
    { "__tostring", tixmldocument_repr },
    { "select", tixmldocument_select },
    { "write", tixmldocument_write },
    { "delete", tixmldocument_delete },
    { "add", tixmldocument_add },
    { "save", tixmldocument_save },
    { "root", tixmldocument_getroot},
    { NULL, NULL }
  };


  static const struct luaL_reg TiXmlNode_methods[] = {
    { "__tostring", tixmlnode_repr },
    { "__gc", tixmlnode_close },
    { "print", tixmlnode_print },
    { "select", tixmlnode_select },
    { "name", tixmlnode_name },
    { "text", tixmlnode_text },
    { "attribute", tixmlnode_getattr },
    { "child", tixmlnode_child },
    { "next", tixmlnode_nextsibling },
    { "expand", tixmlnode_expand },
    { NULL, NULL }
  };


  int luaopen_xml_ltxml(lua_State* L) {
    // create metatable
    luaL_newmetatable(L, "TiXmlDocument");
    // metatable.__index = metatable
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    // register methods
    luaL_register(L, NULL, TiXmlDocument_methods);

    luaL_newmetatable(L, "TiXmlNode");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, TiXmlNode_methods);

    // create registry table for documents
    // 1) create documents table
    lua_createtable(L, 0, 10); // preallocate 10 key-value pairs
    // 2) create metatable for DOCUMENTS_INDEX
    //    DOCUMENTS_INDEX will be a weak table
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "v");
    lua_setfield(L, -2, "__mode");
    // 3) set metatable
    lua_setmetatable(L, -2);
    DOCUMENTS_INDEX = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_createtable(L, 0, 10);
    DOCUMENTS_REFCOUNT_INDEX = luaL_ref(L, LUA_REGISTRYINDEX);

    // register functions
    luaL_register(L, "xml", TinyXML);

    return 1;
  }
}
