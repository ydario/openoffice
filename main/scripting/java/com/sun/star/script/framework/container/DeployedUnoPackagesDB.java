/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



package com.sun.star.script.framework.container;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.ByteArrayInputStream;

import java.util.ArrayList;

// import javax.xml.parsers.DocumentBuilderFactory;
// import javax.xml.parsers.DocumentBuilder;
// import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.*;

public class DeployedUnoPackagesDB {

    // File name to be used for parcel descriptor files
    private static final String
        PARCEL_DESCRIPTOR_NAME = "unopkg-desc.xml";

    // This is the default contents of a parcel descriptor to be used when
    // creating empty descriptors
    private static final byte[] EMPTY_DOCUMENT =
        ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
         "<unopackages xmlns:unopackages=\"unopackages.dtd\">\n" +
         "</unopackages>").getBytes();

    private File file = null;
    private Document document = null;

    public DeployedUnoPackagesDB() throws IOException {
        ByteArrayInputStream bis = null;
        try {
            bis = new ByteArrayInputStream(EMPTY_DOCUMENT);
            this.document = XMLParserFactory.getParser().parse(bis);
        }
        finally {
            if (bis != null)
                bis.close();
        }
    }

    public DeployedUnoPackagesDB(Document document) {
        this.document = document;
    }

    public DeployedUnoPackagesDB(InputStream is) throws IOException {
        this(XMLParserFactory.getParser().parse(is));
    }

    public String[] getDeployedPackages( String language )
    {
        ArrayList packageUrls = new ArrayList(4); 
        Element main = document.getDocumentElement();
        Element root = null;
        Element item;
        int len = 0; 
        NodeList langNodes = null;

        if ((langNodes = main.getElementsByTagName("language")) != null &&
            (len = langNodes.getLength()) != 0)
        {
            for ( int i=0; i<len; i++ )
            {
                Element e = (Element)langNodes.item( i ); 
                if ( e.getAttribute("value").equals(language) )
                {
                    root = e;
                    break;
                }
            }
        }
        if ( root != null )
        {
            len = 0;
            NodeList packages = null;
            if ((packages = root.getElementsByTagName("package")) != null &&
                (len = packages.getLength()) != 0)
            {
   
                for ( int i=0; i<len; i++ )
                {

                    Element e = (Element)packages.item( i ); 
                    packageUrls.add( e.getAttribute("value") ); 
                }
            }
        }
        if ( !packageUrls.isEmpty() )
        {
            return (String[])packageUrls.toArray( new String[0] );
        }
        return new String[0];
    }

    public void write(OutputStream out) throws IOException {
        XMLParserFactory.getParser().write(document, out);
    }

    public Document getDocument() {
        return document;
    }


    private void clearEntries() {
        NodeList langNodes;
        Element main = document.getDocumentElement();
        int len;

        if ((langNodes = document.getElementsByTagName("language")) != null &&
            (len = langNodes.getLength()) != 0)
        {
            for (int i = len - 1; i >= 0; i--) {
                try {
                    main.removeChild(langNodes.item(i));
                }
                catch (DOMException de) {
                    // ignore
                }
            }   
        }
    }
        
    public boolean removePackage( String language, String url )
    {
        Element main = document.getDocumentElement();
        Element langNode = null;
        int len = 0; 
        NodeList langNodes = null;
        boolean result = false;
        if ((langNodes = main.getElementsByTagName("language")) != null &&
            (len = langNodes.getLength()) != 0)
        {
            for ( int i=0; i<len; i++ )
            {
                Element e = (Element)langNodes.item( i ); 
                if ( e.getAttribute("value").equals(language) )
                {
                    langNode = e;
                    break;
                }
            }
        }
        if ( langNode != null )
        {
            len = 0;
            NodeList packages = null;
            if ((packages = langNode.getElementsByTagName("package")) != null &&
                (len = packages.getLength()) != 0)
            {
                for ( int i=0; i<len; i++ )
                {

                    Element e = (Element)packages.item( i ); 
                    String value =  e.getAttribute("value");
                    
                    if ( value.equals(url) )
                    {
                        langNode.removeChild( e );
                        result = true;
                        break;
                    }
                }
            }
        }
        return result;
    }

    public void addPackage(String language, String url ) {
        Element main = document.getDocumentElement();
        Element langNode = null;
        Element pkgNode = null;

        int len = 0; 
        NodeList langNodes = null;
         
        if ((langNodes = document.getElementsByTagName("language")) != null &&
            (len = langNodes.getLength()) != 0)
        {
            for ( int i=0; i<len; i++ )
            {
                Element e = (Element)langNodes.item( i ); 
                if ( e.getAttribute("value").equals(language) )
                {
                    langNode = e;
                    break;
                }
            }
        }
        if ( langNode == null )
        {
            langNode = document.createElement("language");
            langNode.setAttribute( "value", language ); 
        }
        pkgNode = document.createElement("package");
        pkgNode.setAttribute( "value", url ); 

        langNode.appendChild(pkgNode);
        //add to the Top Element
        main.appendChild(langNode);
    }
}
