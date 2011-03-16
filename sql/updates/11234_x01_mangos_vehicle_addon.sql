-- cleanup useless rows with default values only
delete from creature_extended where size=0 and minloot=0 and SpellDmgMulti=1 and XPMulti=1 and honor=0;

ALTER TABLE creature_extended
  ADD COLUMN vehicle_id smallint(5) unsigned NOT NULL DEFAULT '0' AFTER honor,
  ADD COLUMN passengers text AFTER honor;

insert into creature_extended (entry,vehicle_id,passengers) select entry,vehicle_id,passengers from creature_template_addon where vehicle_id != 0;

ALTER TABLE creature_addon DROP COLUMN vehicle_id, DROP COLUMN passengers;
ALTER TABLE creature_template_addon DROP COLUMN vehicle_id, DROP COLUMN passengers;
