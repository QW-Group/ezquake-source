<h2>Writing XHTML markup</h2>

<p>The manual pages you can edit using this tool must be written in eXtensible
HyperText Markup Language 1.1 - XHTML&nbsp;1.1.
If you are not sure what does this require, check following guide.</p>

<p>Probably you know about HTML elements, how to use them.</p>

<pre><code>&lt;p&gt;Paragraph text enclosed inside pair 'p' element.&lt;/p&gt;
&lt;p&gt;Another paragraph with &lt;br /&gt; a newline mark.&lt;/p&gt;</code></pre>

<p>You cannot use characters <code>&lt;, &gt;</code> in the markup freely, because they surrond elements.
You have to replace such special characters with special string we call 'entities':</p>

<table>
<thead><tr><td>char</td><td>entity</td></tr></thead>
<tbody>
<tr><td>&lt;</td><td>&amp;lt;</td></tr>
<tr><td>&gt;</td><td>&amp;gt;</td></tr>
<tr><td>&amp;</td><td>&amp;amp;</td></tr>
</tbody>
</table>

<p>So the first rule is:<br />
<strong>Pair tags, like &lt;p&gt;, must be closed.</strong></p>

<p>Your text shouldn't just float around, so the second rule is:<br />
<strong>Enclose your text inside paragraphs.</strong></p>

<p>There is a difference between the 1<sup>st</sup> and 2<sup>nd</sup> rule.<br />
If you break first rule, document won't be <strong>well-formed</strong>. That means browser viewing
the page will get confused because e.g. somewhere you forgot to place closing tag for something.
We are using very modern mime-type and modern browsers like Opera or Firefox will
display errors within the page.<br />
Before submitting the page your markup will get checked and you should get error message when
it's not well-formed.<br />
If you break the second rule, the document won't be <strong>valid</strong>.
It's not such a issue as not well-formed document, but still shouldn't happen.
To know how to write valid documents you must learn XHTML 1.1 language.
This guide should explain you the subgroup of XHTML 1.1 you need to write structured,
rich and valid documents without having to learn whole XHTML 1.1 specification.</p>

<h3>Paragraphs in detail</h3>

<p>Paragraphs are a simpler affair, because there is only one version.
Text existing between the opening <code>&lt;p&gt;</code> and closing
<code>&lt;/p&gt;</code> tags is formatted just like on a word processor.</p>

<pre>
&lt;h3&gt;A lesson in paragraphs&lt;/h3&gt;
&lt;p&gt;This paragraph follows an h3 heading.
It will be the first time I will get to see
something that looks like a REAL web page,
although I'll have seen something similar
in a wordprocessor document.&lt;/p&gt;
&lt;p&gt;The second paragraph will be just
as boring as the first, but it will
demonstrate the fact that a line break is
added between the two paragraph blocks. What
will follow will be another heading.&lt;/p&gt;
&lt;h3&gt;Another, unremarkable heading&lt;/h3&gt;
&lt;p&gt;The final paragraph demonstrates
that a line break is automatically inserted
after a heading as well. It means that the
document looks well presented. Notice the
font that this is being shown in. It is the
DEFAULT font, chosen by your browser
(usually Times New Roman).&lt;/p&gt;
</pre>

<p>This is how it will look:</p>

<div class="codeexample">
<h3>A lesson in paragraphs</h3>
<p>This paragraph follows an h3 heading.
It will be the first time I will get to
see something that looks like a REAL
web page, although I'll have seen
something similar in a wordprocessor
document.</p>
<p>The second paragraph will be just as
boring as the first, but it will
demonstrate the fact that a line break
is added between the two paragraph blocks.
What will follow will be another heading.</p>
<h3>Another, unremarkable heading</h3>
<p>The final paragraph demonstrates that
a line break is automatically inserted
after a heading as well. It means that the
document looks well presented. Notice the
font that this is being shown in. It is the
DEFAULT font, chosen by your browser
(usually Times New Roman).</p>
</div>

<p>Okay, you are doing really well. This is one of the longest lessons
with perhaps the steepest learning curve. You have one more series of
tags to learn that will mean you will be armed with the most important
structure tags in <acronym title="Extensible Hypertext Markup Language">XHTML</acronym>.</p>

<h3>Lists (3 different kinds)</h3>

<p>After paragraphs and headings, lists are probably the most common
<acronym title="Extensible Hypertext Markup Language">XHTML</acronym> element. There are three,
basic types of list, which I will ... er ... list for you:-</p>

<ul>
  <li>Unordered List (like this one)</li>
  <li>Ordered List</li>
  <li>Glossary List</li>
</ul>

<h3>The Unordered List</h3>

<p>This type of list opens with the <code>&lt;ul&gt;</code> tag
and closes with the <code>&lt;/ul&gt;</code> tag. You have had
some experience with opening and closing tags so this should not
come as any surprise.</p>

<p>Each item of the list also has its <em>own</em> opening and
closing tag. <code>&lt;li&gt;</code> opens the item and
(surprise, surprise) <code>&lt;/li&gt;</code> closes it.</p>

<p>The best way to see the unordered list in action is to try it.
Using the well-used template, create a file called
<code>page04.html</code> that includes the following code
within the file's body:-</p>

<pre>
&lt;ul&gt;
  &lt;li&gt;First list item&lt;/li&gt;
  &lt;li&gt;Second list item&lt;/li&gt;
  &lt;li&gt;Third list item&lt;/li&gt;
  &lt;li&gt;Fourth list item&lt;/li&gt;
&lt;/ul&gt;
</pre>

<p>You will notice that I have indented the list item lines by
two spaces. This makes it easier to see the structure of the list
and becomes particularly useful later when we discuss list nesting.</p>
<p>Viewed in a browser, you should see the following:-</p>

<div class="codeexample">
<ul>
  <li>First list item</li>
  <li>Second list item</li>
  <li>Third list item</li>
  <li>Fourth list item</li>
</ul>
</div>

<p>Pretty easy, huh? Now we will have a go at nesting them and then
you will see why the two spaces were helpful.</p>

<p>Create <code>page05.html</code> with the following:-</p>

<pre>
&lt;ul&gt;
  &lt;li&gt;First list item&lt;/li&gt;
  &lt;li&gt;Second list item
    &lt;ul&gt;
      &lt;li&gt;First nested item&lt;/li&gt;
      &lt;li&gt;Second nested item&lt;/li&gt;
      &lt;li&gt;Third nested item&lt;/li&gt;
    &lt;/ul&gt;
  &lt;/li&gt;
  &lt;li&gt;Third list item&lt;/li&gt;
  &lt;li&gt;Fourth list item&lt;/li&gt;
&lt;/ul&gt;
</pre>

<p>The code should look like this, assuming all the tags are in the
correct places:-</p>

<div class="codeexample">
<ul>
  <li>First list item</li>
  <li>Second list item
    <ul>
      <li>First nested item</li>
      <li>Second nested item</li>
      <li>Third nested item</li>
    </ul>
  </li>
  <li>Third list item</li>
  <li>Fourth list item</li>
</ul>
</div>

<p>You will notice that the bullet point changes for the nested group.
There are several types that are used, the deeper the nesting goes.</p>

<h3>The Ordered List</h3>

<p>This type of list opens with the <code>&lt;ol&gt;</code> tag
and closes with the <code>&lt;/ol&gt;</code> tag. List items use the
same <code>li</code> tag as unordered lists. Create
<code>page06.html</code> with the code below. It
is almost identical to the previous example:-</p>

<pre>
&lt;ol&gt;
  &lt;li&gt;First list item&lt;/li&gt;
  &lt;li&gt;Second list item
    &lt;ol&gt;
      &lt;li&gt;First nested item&lt;/li&gt;
      &lt;li&gt;Second nested item&lt;/li&gt;
      &lt;li&gt;Third nested item&lt;/li&gt;
    &lt;/ol&gt;
  &lt;/li&gt;
  &lt;li&gt;Third list item&lt;/li&gt;
  &lt;li&gt;Fourth list item&lt;/li&gt;
&lt;/ol&gt;
</pre>

<p>The code should look like this, assuming all the tags are in the
correct places:-</p>

<div class="codeexample">
<ol>
  <li>First list item</li>
  <li>Second list item
    <ol>
      <li>First nested item</li>
      <li>Second nested item</li>
      <li>Third nested item</li>
    </ol>
  </li>
  <li>Third list item</li>
  <li>Fourth list item</li>
</ol>
</div>

<p>It is possible to change the numbering system (or bullet in the
case of unordered lists), however this requires the use of Cascading
Style Sheets.</p>

<p>Definition lists (also called <em>Glossary Lists</em> differ slightly from other lists because
each listitem has two parts.</p>

<ul>
  <li>A term</li>
  <li>A term's definition</li>
</ul>

<p>Each part of the glossary has its own tag. <code>&lt;dl&gt;</code>
and <code>&lt;/dl&gt;</code> are used to open and close the list.
<code>&lt;dt&gt;</code> and <code>&lt;/dt&gt;</code> open and close
the &quot;term&quot; part and <code>&lt;dd&gt;</code> and
<code>&lt;/dd&gt;</code> are used to open and close
the &quot;definition&quot; part.</p>

<p>Create a file called <code>page07.html</code> that includes
the following code within the file's body:-</p>

<pre>
&lt;dl&gt;
  &lt;dt&gt;James T. Kirk&lt;/dt&gt;
    &lt;dd&gt;The heroic captain of the USS
    Enterprise has distinguished himself
    countless times when facing the
    unknown.&lt;/dd&gt;
  &lt;dt&gt;Spock&lt;/dt&gt;
    &lt;dd&gt;Kirk's trusted science
    officer can always be depended upon
    for solutions to difficult problems
    as well as unswerving loyalty to his
    commander.&lt;/dd&gt;
  &lt;dt&gt;Montgomery Scott&lt;/dt&gt;
    &lt;dd&gt;The most talented engineer
    in Starfleet, Scotty has used his
    skills to save the USS Enterprise
    and keep her shipshape.&lt;/dd&gt;
&lt;/dl&gt;
</pre>

<p>The code should look like this:-</p>

<div class="codeexample">
<dl>
  <dt>James T. Kirk</dt>
    <dd>The heroic captain of the USS
    Enterprise has distinguished himself
    countless times when facing the
    unknown.</dd>
  <dt>Spock</dt>
    <dd>Kirk's trusted science
    officer can always be depended upon
    for solutions to difficult problems
    as well as unswerving loyalty to his
    commander.</dd>
  <dt>Montgomery Scott</dt>
    <dd>The most talented engineer
    in Starfleet, Scotty has used his
    skills to save the USS Enterprise
    and keep her shipshape.</dd>
</dl>
</div>

<p>Major part of this tutorial has been borrowed from <a href="http://jessey.net/simon/xhtml_tutorial/two.html">Jessey.net</a>.</p>
