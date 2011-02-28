/*
 *    Node map implementation
 *
 * Copyright 2005 Mike McCormack
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#define COBJMACROS

#include <stdarg.h>
#ifdef HAVE_LIBXML2
# include <libxml/parser.h>
# include <libxml/xmlerror.h>
#endif

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winnls.h"
#include "ole2.h"
#include "msxml6.h"

#include "msxml_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msxml);

#ifdef HAVE_LIBXML2

typedef struct _xmlnodemap
{
    IXMLDOMNamedNodeMap IXMLDOMNamedNodeMap_iface;
    ISupportErrorInfo ISupportErrorInfo_iface;
    LONG ref;
    IXMLDOMNode *node;
    LONG iterator;
} xmlnodemap;

static inline xmlnodemap *impl_from_IXMLDOMNamedNodeMap( IXMLDOMNamedNodeMap *iface )
{
    return CONTAINING_RECORD(iface, xmlnodemap, IXMLDOMNamedNodeMap_iface);
}

static inline xmlnodemap *impl_from_ISupportErrorInfo( ISupportErrorInfo *iface )
{
    return CONTAINING_RECORD(iface, xmlnodemap, ISupportErrorInfo_iface);
}

static HRESULT WINAPI xmlnodemap_QueryInterface(
    IXMLDOMNamedNodeMap *iface,
    REFIID riid, void** ppvObject )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    TRACE("(%p)->(%s %p)\n", iface, debugstr_guid(riid), ppvObject);

    if( IsEqualGUID( riid, &IID_IUnknown ) ||
        IsEqualGUID( riid, &IID_IDispatch ) ||
        IsEqualGUID( riid, &IID_IXMLDOMNamedNodeMap ) )
    {
        *ppvObject = iface;
    }
    else if( IsEqualGUID( riid, &IID_ISupportErrorInfo ))
    {
        *ppvObject = &This->ISupportErrorInfo_iface;
    }
    else
    {
        FIXME("interface %s not implemented\n", debugstr_guid(riid));
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    IXMLDOMElement_AddRef( iface );

    return S_OK;
}

static ULONG WINAPI xmlnodemap_AddRef(
    IXMLDOMNamedNodeMap *iface )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    return InterlockedIncrement( &This->ref );
}

static ULONG WINAPI xmlnodemap_Release(
    IXMLDOMNamedNodeMap *iface )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    ULONG ref;

    ref = InterlockedDecrement( &This->ref );
    if ( ref == 0 )
    {
        IXMLDOMNode_Release( This->node );
        heap_free( This );
    }

    return ref;
}

static HRESULT WINAPI xmlnodemap_GetTypeInfoCount(
    IXMLDOMNamedNodeMap *iface,
    UINT* pctinfo )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;

    return S_OK;
}

static HRESULT WINAPI xmlnodemap_GetTypeInfo(
    IXMLDOMNamedNodeMap *iface,
    UINT iTInfo, LCID lcid,
    ITypeInfo** ppTInfo )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);
    return get_typeinfo(IXMLDOMNamedNodeMap_tid, ppTInfo);
}

static HRESULT WINAPI xmlnodemap_GetIDsOfNames(
    IXMLDOMNamedNodeMap *iface,
    REFIID riid, LPOLESTR* rgszNames,
    UINT cNames, LCID lcid, DISPID* rgDispId )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
          lcid, rgDispId);

    if(!rgszNames || cNames == 0 || !rgDispId)
        return E_INVALIDARG;

    hr = get_typeinfo(IXMLDOMNamedNodeMap_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI xmlnodemap_Invoke(
    IXMLDOMNamedNodeMap *iface,
    DISPID dispIdMember, REFIID riid, LCID lcid,
    WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult,
    EXCEPINFO* pExcepInfo, UINT* puArgErr )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hr = get_typeinfo(IXMLDOMNamedNodeMap_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(typeinfo, &This->IXMLDOMNamedNodeMap_iface, dispIdMember, wFlags,
                pDispParams, pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

xmlChar *xmlChar_from_wchar( LPCWSTR str )
{
    DWORD len;
    xmlChar *xmlstr;

    len = WideCharToMultiByte( CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL );
    xmlstr = heap_alloc( len );
    if ( xmlstr )
        WideCharToMultiByte( CP_UTF8, 0, str, -1, (LPSTR) xmlstr, len, NULL, NULL );
    return xmlstr;
}

static HRESULT WINAPI xmlnodemap_getNamedItem(
    IXMLDOMNamedNodeMap *iface,
    BSTR name,
    IXMLDOMNode** namedItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    TRACE("(%p)->(%s %p)\n", This, debugstr_w(name), namedItem );
    return IXMLDOMNamedNodeMap_getQualifiedItem(iface, name, NULL, namedItem);
}

static HRESULT WINAPI xmlnodemap_setNamedItem(
    IXMLDOMNamedNodeMap *iface,
    IXMLDOMNode* newItem,
    IXMLDOMNode** namedItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    xmlNodePtr nodeNew;
    xmlNodePtr node;
    xmlnode *ThisNew;

    TRACE("(%p)->(%p %p)\n", This, newItem, namedItem );

    if(!newItem)
        return E_INVALIDARG;

    if(namedItem) *namedItem = NULL;

    node = xmlNodePtr_from_domnode( This->node, 0 );
    if ( !node )
        return E_FAIL;

    /* Must be an Attribute */
    ThisNew = get_node_obj( newItem );
    if(!ThisNew) {
        FIXME("ThisNew is not our node implementation\n");
        return E_FAIL;
    }

    if(ThisNew->node->type != XML_ATTRIBUTE_NODE)
        return E_FAIL;

    if(!ThisNew->node->parent)
        if(xmldoc_remove_orphan(ThisNew->node->doc, ThisNew->node) != S_OK)
            WARN("%p is not an orphan of %p\n", ThisNew->node, ThisNew->node->doc);

    nodeNew = xmlAddChild(node, ThisNew->node);

    if(namedItem)
        *namedItem = create_node( nodeNew );
    return S_OK;
}

static HRESULT WINAPI xmlnodemap_removeNamedItem(
    IXMLDOMNamedNodeMap *iface,
    BSTR name,
    IXMLDOMNode** namedItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    TRACE("(%p)->(%s %p)\n", This, debugstr_w(name), namedItem );
    return IXMLDOMNamedNodeMap_removeQualifiedItem(iface, name, NULL, namedItem);
}

static HRESULT WINAPI xmlnodemap_get_item(
    IXMLDOMNamedNodeMap *iface,
    LONG index,
    IXMLDOMNode** listItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    xmlNodePtr node;
    xmlAttrPtr curr;
    LONG attrIndex;

    TRACE("(%p)->(%d %p)\n", This, index, listItem);

    *listItem = NULL;

    if (index < 0)
        return S_FALSE;

    node = xmlNodePtr_from_domnode( This->node, 0 );
    curr = node->properties;

    for (attrIndex = 0; attrIndex < index; attrIndex++) {
        if (curr->next == NULL)
            return S_FALSE;
        else
            curr = curr->next;
    }
    
    *listItem = create_node( (xmlNodePtr) curr );

    return S_OK;
}

static HRESULT WINAPI xmlnodemap_get_length(
    IXMLDOMNamedNodeMap *iface,
    LONG *listLength)
{
    xmlNodePtr node;
    xmlAttrPtr first;
    xmlAttrPtr curr;
    LONG attrCount;

    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );

    TRACE("(%p)->(%p)\n", This, listLength);

    if( !listLength )
        return E_INVALIDARG;

    node = xmlNodePtr_from_domnode( This->node, 0 );
    if ( !node )
        return E_FAIL;

    first = node->properties;
    if (first == NULL) {
	*listLength = 0;
	return S_OK;
    }

    curr = first;
    attrCount = 1;
    while (curr->next) {
        attrCount++;
        curr = curr->next;
    }
    *listLength = attrCount;
 
    return S_OK;
}

static HRESULT WINAPI xmlnodemap_getQualifiedItem(
    IXMLDOMNamedNodeMap *iface,
    BSTR baseName,
    BSTR namespaceURI,
    IXMLDOMNode** qualifiedItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    xmlAttrPtr attr;
    xmlNodePtr node;
    xmlChar *href;
    xmlChar *name;

    TRACE("(%p)->(%s %s %p)\n", This, debugstr_w(baseName), debugstr_w(namespaceURI), qualifiedItem);

    if (!baseName || !qualifiedItem) return E_INVALIDARG;

    node = xmlNodePtr_from_domnode(This->node, XML_ELEMENT_NODE);
    if ( !node )
        return E_FAIL;

    if (namespaceURI && *namespaceURI)
    {
        href = xmlChar_from_wchar(namespaceURI);
        if (!href) return E_OUTOFMEMORY;
    }
    else
        href = NULL;

    name = xmlChar_from_wchar(baseName);
    if (!name)
    {
        heap_free(href);
        return E_OUTOFMEMORY;
    }

    attr = xmlHasNsProp(node, name, href);

    heap_free(name);
    heap_free(href);

    if (!attr)
    {
        *qualifiedItem = NULL;
        return S_FALSE;
    }

    *qualifiedItem = create_node((xmlNodePtr)attr);

    return S_OK;
}

static HRESULT WINAPI xmlnodemap_removeQualifiedItem(
    IXMLDOMNamedNodeMap *iface,
    BSTR baseName,
    BSTR namespaceURI,
    IXMLDOMNode** qualifiedItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    xmlAttrPtr attr;
    xmlNodePtr node;
    xmlChar *name;
    xmlChar *href;

    TRACE("(%p)->(%s %s %p)\n", This, debugstr_w(baseName), debugstr_w(namespaceURI), qualifiedItem);

    if (!baseName) return E_INVALIDARG;

    node = xmlNodePtr_from_domnode( This->node, XML_ELEMENT_NODE );
    if ( !node )
        return E_FAIL;

    if (namespaceURI && *namespaceURI)
    {
        href = xmlChar_from_wchar(namespaceURI);
        if (!href) return E_OUTOFMEMORY;
    }
    else
        href = NULL;

    name = xmlChar_from_wchar(baseName);
    if (!name)
    {
        heap_free(href);
        return E_OUTOFMEMORY;
    }

    attr = xmlHasNsProp( node, name, href );

    heap_free(name);
    heap_free(href);

    if ( !attr )
    {
        if (qualifiedItem) *qualifiedItem = NULL;
        return S_FALSE;
    }

    if ( qualifiedItem )
    {
        xmlUnlinkNode( (xmlNodePtr) attr );
        xmldoc_add_orphan( attr->doc, (xmlNodePtr) attr );
        *qualifiedItem = create_node( (xmlNodePtr) attr );
    }
    else
    {
        if (xmlRemoveProp(attr) == -1)
            ERR("xmlRemoveProp failed\n");
    }

    return S_OK;
}

static HRESULT WINAPI xmlnodemap_nextNode(
    IXMLDOMNamedNodeMap *iface,
    IXMLDOMNode** nextItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    xmlNodePtr node;
    xmlAttrPtr curr;
    LONG attrIndex;

    TRACE("(%p)->(%p: %d)\n", This, nextItem, This->iterator);

    *nextItem = NULL;

    node = xmlNodePtr_from_domnode( This->node, 0 );
    curr = node->properties;

    for (attrIndex = 0; attrIndex < This->iterator; attrIndex++) {
        if (curr->next == NULL)
            return S_FALSE;
        else
            curr = curr->next;
    }

    This->iterator++;

    *nextItem = create_node( (xmlNodePtr) curr );

    return S_OK;
}

static HRESULT WINAPI xmlnodemap_reset(
    IXMLDOMNamedNodeMap *iface )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );

    TRACE("(%p: %d)\n", This, This->iterator);

    This->iterator = 0;

    return S_OK;
}

static HRESULT WINAPI xmlnodemap__newEnum(
    IXMLDOMNamedNodeMap *iface,
    IUnknown** ppUnk)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    FIXME("(%p)->(%p)\n", This, ppUnk);
    return E_NOTIMPL;
}

static const struct IXMLDOMNamedNodeMapVtbl xmlnodemap_vtbl =
{
    xmlnodemap_QueryInterface,
    xmlnodemap_AddRef,
    xmlnodemap_Release,
    xmlnodemap_GetTypeInfoCount,
    xmlnodemap_GetTypeInfo,
    xmlnodemap_GetIDsOfNames,
    xmlnodemap_Invoke,
    xmlnodemap_getNamedItem,
    xmlnodemap_setNamedItem,
    xmlnodemap_removeNamedItem,
    xmlnodemap_get_item,
    xmlnodemap_get_length,
    xmlnodemap_getQualifiedItem,
    xmlnodemap_removeQualifiedItem,
    xmlnodemap_nextNode,
    xmlnodemap_reset,
    xmlnodemap__newEnum,
};

static HRESULT WINAPI support_error_QueryInterface(
    ISupportErrorInfo *iface,
    REFIID riid, void** ppvObject )
{
    xmlnodemap *This = impl_from_ISupportErrorInfo( iface );
    TRACE("%p %s %p\n", iface, debugstr_guid(riid), ppvObject);
    return IXMLDOMNamedNodeMap_QueryInterface(&This->IXMLDOMNamedNodeMap_iface, riid, ppvObject);
}

static ULONG WINAPI support_error_AddRef(
    ISupportErrorInfo *iface )
{
    xmlnodemap *This = impl_from_ISupportErrorInfo( iface );
    return IXMLDOMNamedNodeMap_AddRef(&This->IXMLDOMNamedNodeMap_iface);
}

static ULONG WINAPI support_error_Release(
    ISupportErrorInfo *iface )
{
    xmlnodemap *This = impl_from_ISupportErrorInfo( iface );
    return IXMLDOMNamedNodeMap_Release(&This->IXMLDOMNamedNodeMap_iface);
}

static HRESULT WINAPI support_error_InterfaceSupportsErrorInfo(
    ISupportErrorInfo *iface,
    REFIID riid )
{
    FIXME("(%p)->(%s)\n", iface, debugstr_guid(riid));
    return S_FALSE;
}

static const struct ISupportErrorInfoVtbl support_error_vtbl =
{
    support_error_QueryInterface,
    support_error_AddRef,
    support_error_Release,
    support_error_InterfaceSupportsErrorInfo
};

IXMLDOMNamedNodeMap *create_nodemap( IXMLDOMNode *node )
{
    xmlnodemap *nodemap;

    nodemap = heap_alloc( sizeof *nodemap );
    if ( !nodemap )
        return NULL;

    nodemap->IXMLDOMNamedNodeMap_iface.lpVtbl = &xmlnodemap_vtbl;
    nodemap->ISupportErrorInfo_iface.lpVtbl = &support_error_vtbl;
    nodemap->node = node;
    nodemap->ref = 1;
    nodemap->iterator = 0;

    IXMLDOMNode_AddRef( node );
    /* Since we AddRef a node here, we don't need to call xmldoc_add_ref() */

    return &nodemap->IXMLDOMNamedNodeMap_iface;
}

#endif
