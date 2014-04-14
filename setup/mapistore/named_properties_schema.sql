--
-- Schema used by named_properties MySQL backend (from mapiproxy/mapistore lib) 
--
CREATE TABLE IF NOT EXISTS `named_properties` (
    `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
    `type` TINYINT(1) NOT NULL,
    `propType` INT(10) unsigned NOT NULL,
    `oleguid` VARCHAR(255) NOT NULL,
    `mappedId` INT(10) unsigned NOT NULL,
    `propId` INT(10) unsigned DEFAULT NULL,
    `propName` VARCHAR(255) DEFAULT NULL,
    `oom` VARCHAR(255) DEFAULT NULL,
    `canonical` VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY(`id`),
    KEY `named_properties_nappedId` (`mappedId`),
    KEY `named_properties_type_oleguid_propId` (`type`,`oleguid`,`propId`),
    KEY `named_properties_type_oleguid_propName` (`type`,`oleguid`,`propName`)
) ENGINE=InnoDB CHARSET=utf8;