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
-- Table `mapistore_indexes`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mapistore_indexes` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `username` VARCHAR(1024) NOT NULL,
  `next_fmid` VARCHAR(36) NOT NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB;


