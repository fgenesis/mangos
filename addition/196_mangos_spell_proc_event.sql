DELETE FROM `spell_proc_event` WHERE `entry` IN (21084,31801);
INSERT INTO `spell_proc_event` (`entry`,`SchoolMask`) VALUES (21084,1),(31801,1);
UPDATE `spell_proc_event` SET `ppmRate`=0,`SchoolMask`=1 WHERE `entry`=20375;
