DELETE FROM spell_proc_event WHERE entry = 70723;
INSERT INTO `spell_proc_event` (`entry`, `SchoolMask`, `SpellFamilyMaskA0`, `procFlags`, `procEx`) VALUES ('70723', '0', '5', '65536', '2');

DELETE FROM spell_proc_event WHERE entry IN (70718);
INSERT INTO spell_proc_event VALUES (70718, 0x00,  7, 0x00000000, 0x00000000, 0x00000000, 0x00200000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00004000, 0x00000000, 0.000000, 0.000000,  0);
