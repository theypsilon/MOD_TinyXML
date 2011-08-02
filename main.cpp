// The functions contained in this file are pretty dummy
// and are included only as a placeholder. Nevertheless,
// they *will* get included in the shared library if you
// don't remove them :)
//
// Obviously, you 'll have to write yourself the super-duper
// functions to include in the resulting library...
// Also, it's not necessary to write every function in this file.
// Feel free to add more files in this project. They will be
// included in the resulting library.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bgddl.h"


#include "tinyxml.h"
#include "tinystr.h"


extern "C"
{

#include "xstrings.h"

}
    /* ----------------------------------------------------------------- */
    /*
    Prototipos de las funciones de ejemplo
    */

    static int openXMLDocument(INSTANCE * my,int * params);
    static int closeXMLDocument(INSTANCE * my,int * params);
    static int saveXMLDocument(INSTANCE * my,int * params);
    static int firstXMLChild(INSTANCE * my,int * params);
    static int firstXMLChildElement(INSTANCE * my,int * params);
    static int getParent(INSTANCE * my,int * params);
    static int getAttribute(INSTANCE * my,int * params);
    static int getValue(INSTANCE * my,int * params);
    static int nextXMLElement(INSTANCE * my,int * params);
    static int previousXMLElement(INSTANCE * my,int * params);
    static int XMLQueryIntAttribute(INSTANCE * my,int * params);
    static int XMLQueryFloatAttribute(INSTANCE * my,int * params);
    static int XMLQueryDoubleAttribute(INSTANCE * my,int * params);
    static int newXMLElement(INSTANCE * my,int * params);
    static int deleteXMLElement(INSTANCE * my,int * params);
    static int setAttributeXMLElement(INSTANCE * my,int * params);
    static int setAttributeIntXMLElement(INSTANCE * my,int * params);

    /* ----------------------------------------------------------------- */
    /* Definicion de constantes (usada en tiempo de compilacion) */

    DLLEXPORT DLCONSTANT __bgdexport(mod_tinyxml,constants_def)[] = {

        { "XML_READ", TYPE_DWORD, 0 },
        { "XML_WRITE" , TYPE_DWORD, 1 },
        { NULL, TYPE_DWORD, 0 }
    } ;

    #define FENIX_export(a,b,c,d) {a,b,c,(void*)d}

    DLLEXPORT DLSYSFUNCS __bgdexport(mod_tinyxml,functions_exports)[] = {
        FENIX_export( "XML_OPEN", "SI", TYPE_DWORD, openXMLDocument ),
        FENIX_export( "XML_CLOSE", "I", TYPE_INT, closeXMLDocument ),
        FENIX_export( "XML_SAVE", "I", TYPE_INT, saveXMLDocument ),
        FENIX_export( "XML_ATTRIBUTE", "IS", TYPE_STRING, getAttribute ),
        FENIX_export( "XML_ATTRIBUTEI", "ISP", TYPE_INT, XMLQueryIntAttribute ),
        FENIX_export( "XML_ATTRIBUTEF", "ISP", TYPE_INT, XMLQueryFloatAttribute ),
        FENIX_export( "XML_ATTRIBUTED", "ISP", TYPE_INT, XMLQueryDoubleAttribute ),
        FENIX_export( "XML_VALUE", "I", TYPE_STRING, getValue ),
        FENIX_export( "XML_NEXTELEMENT", "I", TYPE_DWORD, nextXMLElement ),
        FENIX_export( "XML_PREVIOUSELEMENT", "I", TYPE_DWORD, previousXMLElement ),
        FENIX_export( "XML_FIRSTCHILD", "I", TYPE_DWORD, firstXMLChild ),
        FENIX_export( "XML_FIRSTCHILDNAMED", "IS", TYPE_DWORD, firstXMLChildElement ),
        FENIX_export( "XML_PARENTNODE", "I", TYPE_DWORD, getParent ),
        FENIX_export( "XML_NEWELEMENT", "IS", TYPE_DWORD, newXMLElement ),
        FENIX_export( "XML_DELELEMENT", "I", TYPE_DWORD, deleteXMLElement ),
        FENIX_export( "XML_SET_ATTRIBUTE", "ISS", TYPE_DWORD, setAttributeXMLElement ),
        FENIX_export( "XML_SET_ATTRIBUTE", "ISI", TYPE_DWORD, setAttributeIntXMLElement ),
        { 0 , 0 , 0 , 0 }
    };



static int openXMLDocument(INSTANCE * my,int * params) {
	TiXmlDocument* xmldoc;
	int i = params[1];

	if (i == 1) {
		xmldoc = new TiXmlDocument ();
		TiXmlElement* header = new TiXmlElement("XMLDocumentName");
		header->SetAttribute("name",string_get(params[0]));
		xmldoc->LinkEndChild( header );

		string_discard(params[0]);
	} else
		xmldoc = new TiXmlDocument (string_get(params[0]));

        string_discard(params[0]);
        TiXmlNode* n;

        if (!xmldoc->LoadFile()) {
            delete xmldoc;
            return -1;
        }

        n = xmldoc->FirstChild();

        if ( n == NULL ) {
            delete xmldoc;
            return -1;
        }
    }

    return ((int)n);
}

static int closeXMLDocument(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);

	if (n == NULL)
		return -1;

	TiXmlDocument* xmldoc = n->GetDocument();

	if (xmldoc == NULL)
		return -1;

	delete xmldoc;
	return 0;
}

static int saveXMLDocument(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);

	if (n == NULL)
		return -1;

	TiXmlDocument* xmldoc = n->GetDocument();

	if (xmldoc == NULL)
		return -1;

	TiXmlElement* e = xmldoc->FirstChildElement("XMLDocumentName");

	if ( e == NULL || e->Attribute("name") == NULL)
		return -1;

	char c[1024];
	sprintf(c,"%s",e->Attribute("name"));

	xmldoc->RemoveChild(e);

	if (xmldoc->SaveFile(c)) {
		TiXmlElement* header = new TiXmlElement("XMLDocumentName");
		header->SetAttribute("name",c);
		xmldoc->LinkEndChild( header );
		return 0;
	} else
		return -1;
}

static int firstXMLChild(INSTANCE * my,int * params) {

	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);

	if (n == NULL)
		return 0;

	n = (TiXmlNode*) n->FirstChildElement();

	return ((int)n);
}

static int firstXMLChildElement(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);
	const char* name = string_get(params[1]);
	string_discard(params[1]);

	if (n == NULL)
		return 0;

	n = (TiXmlNode*) n->FirstChildElement(name);

	return ((int)n);
}

int stringError(const char* s) {
	char b[32];
	// sprintf(b,"Error in %s",s);
	sprintf(b,"");
	int ret = string_new(b);

	string_use(ret);

	return (ret);
}

static int getAttribute(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);
	const char* name = string_get(params[1]);
	string_discard(params[1]);

	if (n == NULL)
		return stringError("Attribute 1");

	TiXmlElement* e = n->ToElement();

	if (e == NULL)
		return stringError("Attribute 2");

	const char* atrib = e->Attribute(name);

	if (atrib == NULL)
		return stringError("Attribute 3");

	int ret = string_new(atrib);

	string_use(ret);

	return (ret);
}

static int XMLQueryIntAttribute(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);
	const char* name = string_get(params[1]);
	string_discard(params[1]);
	int* i = (int*) params[2];

	if (n == NULL || i == NULL)
		return -1;

	TiXmlElement* e = n->ToElement();

	if (e == NULL)
		return -1;

	e->QueryIntAttribute(name,i);

	return 0;
}

static int XMLQueryFloatAttribute(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);
	const char* name = string_get(params[1]);
	string_discard(params[1]);
	float* i = (float*) params[2];

	if (n == NULL || i == NULL)
		return -1;

	TiXmlElement* e = n->ToElement();

	if (e == NULL)
		return -1;

	double res;
	e->QueryDoubleAttribute(name,&res);
	*i= res;

	return 0;
}

static int XMLQueryDoubleAttribute(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);
	const char* name = string_get(params[1]);
	string_discard(params[1]);
	double* i = (double*) params[2];

	if (n == NULL || i == NULL)
		return -1;

	TiXmlElement* e = n->ToElement();

	if (e == NULL)
		return -1;

	e->QueryDoubleAttribute(name,i);

	return 0;
}

static int getValue(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);

	if (n == NULL)
		return stringError("Value 1");

	const char* value = n->Value();

	if (value == NULL)
		return stringError("Value 2");

	int ret = string_new(value);

	string_use(ret);

	return (ret);
}

static int nextXMLElement(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);

	if (n == NULL)
		return 0;

	n = (TiXmlNode*) n->NextSiblingElement();

	return ((int)n);

}

static int previousXMLElement(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);

	if (n == NULL)
		return 0;

	for (; n ; n = n->PreviousSibling())
		if (n->ToElement())
			return ((int)n);

	return 0;

}

static int getParent(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);

	if (n == NULL)
		return 0;

	n = (TiXmlNode*) n->Parent();

	if (n!= NULL && n->ToElement())
		return ((int)n);
	else
		return 0;
}

static int newXMLElement(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);
	const char* name = string_get(params[1]);
	string_discard(params[1]);

	if (n == NULL)
		return 0;

	TiXmlElement* newE = new TiXmlElement(name);

	n->LinkEndChild(newE);

	return ((int)newE);
}

static int deleteXMLElement(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);

	if (n == NULL)
		return -1;

	TiXmlNode* nParent = (TiXmlNode*) n->Parent();

	if (nParent == NULL)
		return -1;

	nParent->RemoveChild(n);

	return 0;
}

static int setAttributeXMLElement(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);
	const char* name = string_get(params[1]);
	string_discard(params[1]);
	const char* value = string_get(params[2]);
	string_discard(params[2]);

	if (n == NULL || !n->ToElement())
		return -1;

	n->ToElement()->SetAttribute(name,value);

	return 0;
}

static int setAttributeIntXMLElement(INSTANCE * my,int * params) {
	TiXmlNode* n = reinterpret_cast<TiXmlNode*>(params[0]);
	const char* name = string_get(params[1]);
	string_discard(params[1]);

	int value = params[2];

	if (n == NULL || !n->ToElement())
		return -1;

	n->ToElement()->SetAttribute(name,value);

	return 0;
}

