<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>

	<xsl:template match="/command">
		<html>
			<head>
				<title><xsl:value-of select="name"/> command</title>
			</head>
			<body>
				<h1>Command: <xsl:value-of select="name"/></h1>
				<hr/>
				<p><xsl:value-of select="description"/></p>
				<xsl:apply-templates select="syntax"/>
				<xsl:apply-templates select="arguments"/>
				<xsl:apply-templates select="remarks"/>
			</body>
		</html>
	</xsl:template>

	<xsl:template match="remarks">
		<h2>Remarks</h2>
		<p><xsl:value-of select="."/></p>
	</xsl:template>

	<xsl:template match="syntax">
		<h2>Syntax</h2>
		<p><code>
			<xsl:value-of select="/command/name"/>
			<xsl:text> </xsl:text>
			<xsl:value-of select="."/>
		</code></p>
	</xsl:template>

	<xsl:template match="arguments">
		<h2>Arguments</h2>
		<p>
			<ul>
				<xsl:for-each select="argument">
				<li><b><xsl:value-of select="name"/></b>: <xsl:value-of select="description"/></li>
				</xsl:for-each>
			</ul>
		</p>
	</xsl:template>

</xsl:stylesheet>
