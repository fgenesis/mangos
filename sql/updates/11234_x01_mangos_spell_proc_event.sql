DELETE FROM spell_chain WHERE spell_id IN (57529, 57531);
INSERT INTO spell_chain VALUES
(57529, 0, 57529, 1, 0),
(57531, 57529, 57529, 2, 0);

DELETE FROM spell_proc_event WHERE entry IN (31571, 31572, 57529, 57531);
INSERT INTO spell_proc_event VALUES (31571, 0x00,  3, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00004000, 0x00000000, 0.000000, 0.000000,  0);
INSERT INTO spell_proc_event VALUES (57529, 0x00,  3, 0x61620035, 0x00000000, 0x00000000, 0x00001000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00011110, 0x00000000, 0.000000, 100.000000,  0);
