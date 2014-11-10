-- -----------------------------------------------------
-- Table `mapistore_indexing`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mapistore_indexing` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `username` VARCHAR(1024) NOT NULL,
  `fmid` VARCHAR(36) NOT NULL,
  `url` VARCHAR(1024) NOT NULL,
  `soft_deleted` VARCHAR(36) NOT NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB;

-- -----------------------------------------------------
-- Indexes from `mapistore_indexing`
-- -----------------------------------------------------
CREATE INDEX `mapistore_indexing_username_idx` ON `mapistore_indexing`
  (`username`(255) ASC);

CREATE INDEX `mapistore_indexing_username_url_idx` ON `mapistore_indexing`
  (`username`(255) ASC, `url`(255) ASC);

CREATE INDEX `mapistore_indexing_username_fmid_idx` ON `mapistore_indexing`
  (`username`(255) ASC, `fmid` ASC);

-- -----------------------------------------------------
-- Table `mapistore_indexes`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mapistore_indexes` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `username` VARCHAR(1024) NOT NULL,
  `next_fmid` VARCHAR(36) NOT NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB;

-- -----------------------------------------------------
-- Indexes from `mapistore_indexes`
-- -----------------------------------------------------
CREATE INDEX `mapistore_indexes_username_idx` ON `mapistore_indexes`
  (`username`(255) ASC);
