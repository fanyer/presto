Documentation for the ACTION system

rikard@opera.com, 2005-11-24

Actions are now defined in the modules/products/platforms where they are
implemented. Just put a file named "module.actions" in the top directory of
your module (eg platforms/windows, adjunct/quick, modules/display). This file
will be looked for and parsed during compile time with a Pike script.

Product configurators now need to decide which actions that they want. Only
those that are in the product's configuration and feature set need to be
decided on.

Embedded products will have to decide on substantially fewer actions than
desktop products, for example.

Why is this better?

Because now you don't need to modify the inputmanager module to add new actions.
And you don't need to modify the inputmanager module to ifdeff away those that you
don't want, either. Just turn them off in your configuration file.

What do I need to do?

As a module/product/platform owner:

Please take a look in hardcore/module.actions. If you find actions there that
you should own, please move them to a module.actions on the top level of your
module/platform/product.

If actions are defined in several places they will be merged. Two actions with
the same name must have identical strings or the offending ones will be
ignored.

Product owners only:

You will get compile errors if all actions in your feature set are not decided
on. You should grab hardcore/actions/generated_actions_template.h, put it where
you'd like it and answer YES or NO to the actions.

This file needs to be included in the compilation somehow. Recommended is that
you tell me where you call it and it can then be included from
hardcore/actions/product_actionfiles.h .

Alternatively, you can do a #define PRODUCT_ACTIONS_FILE yourfilename.h
sufficiently early. You may in that case need to do it in the build system.

File format:

The actions in the module.actions files are on the following format:

ACTION_NAME                                        owner
	String:		"Generic action"
	Depends on:	FEATURE_MIFF_MIFF, MOFFA_SUPPORT

String: is used for parsing the action from the input config files.

Depends on: can contain either FEATURE defines (preferred) or regular ifdeffs
(for things that are not yet featurised). Multiple features/ifdeffs are
separated with logical "," :-). (They will be AND:ed.)

Actions with the same names but from different entries (possibly different
files) will be OR:ed.
