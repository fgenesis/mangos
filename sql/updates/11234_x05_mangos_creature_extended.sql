ALTER TABLE creature_extended
  ADD COLUMN stayInAir smallint(5) unsigned NOT NULL DEFAULT '0' AFTER XPMulti;
  
INSERT IGNORE INTO creature_extended (entry) VALUES (36597);
UPDATE creature_extended SET stayInAir = 1 WHERE entry = 36597; -- Lich King
