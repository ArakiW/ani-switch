# XML layout files

Each activity declares a `CONTENT_FROM_XML_RES("activity/foo.xml")` content
factory. The current v0.1 activities build their real UI programmatically in
`onContentAvailable()`, but Borealis still calls the factory before that
callback. Every activity therefore has a minimal `<brls:Box />` placeholder in
this directory; the callback replaces it with the programmatic view tree.

When an activity moves to a designer-authored layout, replace its placeholder
with the actual XML and update the activity callback accordingly.
