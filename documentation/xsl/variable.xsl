<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>

	<xsl:template match="/variable">
		<html>
			<head>
				<title><xsl:value-of select="name"/> variable</title>
			</head>
			<body>
				<h1>Variable: <xsl:value-of select="name"/></h1>
				<hr/>
				<p><xsl:value-of select="description"/></p>
				<xsl:apply-templates select="value"/>
				<xsl:apply-templates select="remarks"/>
			</body>
		</html>
	</xsl:template>

	<xsl:template match="remarks">
		<h2>Remarks</h2>
		<p><xsl:value-of select="."/></p>
	</xsl:template>

	<xsl:template match="value">
		<h2>Value</h2>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="string">
		<p><xsl:value-of select="."/></p>
	</xsl:template>
	
	<xsl:template match="integer">
		<p><xsl:value-of select="."/></p>
	</xsl:template>
	
	<xsl:template match="float">
		<p><xsl:value-of select="."/></p>
	</xsl:template>
	
	<xsl:template match="boolean">
		<p>
			<ul>
				<li><b>true</b>: <xsl:value-of select="true"/></li>
				<li><b>false</b>: <xsl:value-of select="false"/></li>
			</ul>
		</p>
	</xsl:template>
	
	<xsl:template match="enum">
		<p>
			<ul>
				<xsl:for-each select="value">
				<li><b><xsl:value-of select="name"/></b>: <xsl:value-of select="description"/></li>
				</xsl:for-each>
			</ul>
		</p>
	</xsl:template>
	
</xsl:stylesheet>
