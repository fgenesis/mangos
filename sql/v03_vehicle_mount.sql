ALTER TABLE creature_template
  ADD COLUMN `VehicleEntry` mediumint(8) unsigned NOT NULL default '0' AFTER `PetSpellDataId`;

-- Mechano-hog, Mekgineer's Chopper
UPDATE creature_template SET VehicleEntry = 318, IconName = 'vehichleCursor' WHERE entry IN (29929, 32286);

-- Traveler's Tundra Mammoth, Grand Ice Mammoth, Grand Black War Mammoth, Grand Caravan Mammoth
UPDATE creature_template SET VehicleEntry = 312, IconName = 'vehichleCursor' WHERE entry IN (32633, 32640, 31857, 31858, 31861, 31862, 32212, 32213);

-- X-53 Touring Rocket
UPDATE creature_template SET VehicleEntry = 774, IconName = 'vehichleCursor' WHERE entry = 40725;
