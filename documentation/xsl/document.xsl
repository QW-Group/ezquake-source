<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>

	<xsl:template match="/document">
		<html>
			<head>
				<xsl:apply-templates select="head"/>
			</head>
			<body>
				<h1>
					<font face="arial, verdana">
					<xsl:apply-templates select="head/title"/>
					</font>
				</h1>
				<hr/>
				<xsl:apply-templates select="body"/>
			</body>
		</html>
	</xsl:template>

	<xsl:template match="head">
		<head>
			<title><xsl:value-of select="title"/></title>
		</head>
	</xsl:template>

	<xsl:template match="list">
		<ul>
			<xsl:for-each select="li">
				<li><xsl:apply-templates/></li>
			</xsl:for-each>
		</ul>
	</xsl:template>
	
	<xsl:template match="h">
		<h2>
			<xsl:apply-templates/>
		</h2>
	</xsl:template>
	
	<xsl:template match="p">
		<p>
			<xsl:apply-templates/>
		</p>
	</xsl:template>

	<xsl:template match="pre"	>
		<pre>
			<xsl:apply-templates/>
		</pre>
	</xsl:template>

	<xsl:template match="em">
		<em>
			<xsl:apply-templates/>
		</em>
	</xsl:template>
	
	<xsl:template match="a">
		<a>
			<xsl:attribute name="href"><xsl:value-of select="@href"/></xsl:attribute>
			<xsl:apply-templates/>
		</a>
	</xsl:template>
	
	<xsl:template match="text()">
			<xsl:value-of select="."/>
	</xsl:template>
	
</xsl:stylesheet>
