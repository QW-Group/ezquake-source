<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes"/>

	<xsl:template match="/variable">
		<xsl:element name="div">
				<h4>Description</h4>
				<p><xsl:value-of select="description"/></p>
				<xsl:apply-templates select="value"/>
				<xsl:apply-templates select="remarks"/>
		</xsl:element>
	</xsl:template>

	<xsl:template match="remarks">
		<h4>Remarks</h4>
		<p><xsl:value-of select="."/></p>
	</xsl:template>

	<xsl:template match="value">
		<h4>Value</h4>
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="string">
		<p>String. <xsl:value-of select="."/></p>
	</xsl:template>
	
	<xsl:template match="integer">
		<p>Integer. <xsl:value-of select="."/></p>
	</xsl:template>
	
	<xsl:template match="float">
		<p>Float. <xsl:value-of select="."/></p>
	</xsl:template>
	
	<xsl:template match="boolean">
		<table class="values"><thead><tr><td>value</td><td>description</td></tr></thead>
    <tbody>
      <tr><td>0 = False</td><td><xsl:value-of select="false"/></td></tr>
      <tr><td>1 = True</td><td><xsl:value-of select="true"/></td></tr>
    </tbody>
    </table>
	</xsl:template>
	
	<xsl:template match="enum">
		<table class="values"><thead><tr><td>value</td><td>description</td></tr></thead>
    <tbody>
		  <xsl:for-each select="value">
			  <tr><td><xsl:value-of select="name"/></td><td><xsl:value-of select="description"/></td></tr>
			</xsl:for-each>
		</tbody>
		</table>
	</xsl:template>
	
</xsl:stylesheet>

