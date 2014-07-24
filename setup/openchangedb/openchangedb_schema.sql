-- -----------------------------------------------------
-- Table `company`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `company` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `domain` VARCHAR(1024) NOT NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB;


-- -----------------------------------------------------
-- Table `organizational_units`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `organizational_units` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `company_id` INT NOT NULL,
  `organization` VARCHAR(256) NULL,
  `administrative_group` VARCHAR(256) NULL,
  PRIMARY KEY (`id`),
  CONSTRAINT `fk_organizational_units_company_id`
    FOREIGN KEY (`company_id`)
    REFERENCES `company` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;

CREATE INDEX `fk_organizational_units_company_id_idx` ON `organizational_units` (`company_id` ASC);


-- -----------------------------------------------------
-- Table `public_folders`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `public_folders` (
  `ou_id` INT NOT NULL,
  `ReplicaID` INT NULL,
  `StoreGUID` VARCHAR(36) NULL,
  PRIMARY KEY (`ou_id`),
  CONSTRAINT `fk_public_folders_ou_id`
    FOREIGN KEY (`ou_id`)
    REFERENCES `organizational_units` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;


-- -----------------------------------------------------
-- Table `mailboxes`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mailboxes` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `ou_id` INT NOT NULL,
  `folder_id` BIGINT UNSIGNED NOT NULL,
  `name` VARCHAR(256) NOT NULL,
  `MailboxGUID` VARCHAR(36) NOT NULL,
  `ReplicaGUID` VARCHAR(36) NOT NULL,
  `ReplicaID` INT NOT NULL,
  `SystemIdx` INT NOT NULL,
  `indexing_url` VARCHAR(1024) NULL,
  `locale` VARCHAR(15) NULL,
  PRIMARY KEY (`id`),
  CONSTRAINT `fk_mailboxes_ou_id`
    FOREIGN KEY (`ou_id`)
    REFERENCES `organizational_units` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;

CREATE INDEX `fk_mailboxes_ou_id_idx` ON `mailboxes` (`ou_id` ASC);


-- -----------------------------------------------------
-- Table `folders`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `folders` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `ou_id` INT NOT NULL,
  `folder_id` BIGINT UNSIGNED NOT NULL,
  `folder_class` VARCHAR(6) NOT NULL DEFAULT 'system',
  `mailbox_id` BIGINT UNSIGNED NULL,
  `parent_folder_id` BIGINT UNSIGNED NULL,
  `FolderType` INT NULL,
  `SystemIdx` INT NULL,
  `MAPIStoreURI` VARCHAR(1024) NULL,
  PRIMARY KEY (`id`),
  CONSTRAINT `fk_folders_ou_id`
    FOREIGN KEY (`ou_id`)
    REFERENCES `organizational_units` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE,
  CONSTRAINT `fk_folders_mailbox_id`
    FOREIGN KEY (`mailbox_id`)
    REFERENCES `mailboxes` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE,
  CONSTRAINT `fk_folders_parent_folder_id`
    FOREIGN KEY (`parent_folder_id`)
    REFERENCES `folders` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;

CREATE INDEX `fk_folders_ou_id_idx` ON `folders` (`ou_id` ASC);

CREATE INDEX `fk_folders_mailbox_id_idx` ON `folders` (`mailbox_id` ASC);

CREATE INDEX `fk_folders_parent_folder_id_idx` ON `folders` (`parent_folder_id` ASC);


-- -----------------------------------------------------
-- Table `messages`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `messages` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `ou_id` INT NULL,
  `message_id` BIGINT UNSIGNED NULL,
  `message_type` VARCHAR(45) NULL,
  `folder_id` BIGINT UNSIGNED NULL,
  `mailbox_id` BIGINT UNSIGNED NULL,
  `normalized_subject` TEXT NULL,
  PRIMARY KEY (`id`),
  CONSTRAINT `fk_messages_ou_id`
    FOREIGN KEY (`ou_id`)
    REFERENCES `organizational_units` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE,
  CONSTRAINT `fk_messages_folder_id`
    FOREIGN KEY (`folder_id`)
    REFERENCES `folders` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE,
  CONSTRAINT `fk_messages_mailbox_id`
    FOREIGN KEY (`mailbox_id`)
    REFERENCES `mailboxes` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;

CREATE INDEX `fk_messages_ou_id_idx` ON `messages` (`ou_id` ASC);

CREATE INDEX `fk_messages_folder_id_idx` ON `messages` (`folder_id` ASC);

CREATE INDEX `fk_messages_mailbox_id_idx` ON `messages` (`mailbox_id` ASC);


-- -----------------------------------------------------
-- Table `messages_properties`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `messages_properties` (
  `message_id` BIGINT UNSIGNED NOT NULL,
  `name` VARCHAR(128) NOT NULL,
  `value` VARCHAR(512) NOT NULL,
  CONSTRAINT `fk_messages_properties_message_id`
    FOREIGN KEY (`message_id`)
    REFERENCES `messages` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;

CREATE INDEX `fk_messages_properties_message_id_idx` ON `messages_properties` (`message_id` ASC);

CREATE INDEX `message_properties_message_id_name_idx` ON `messages_properties` (`message_id` ASC, `name` ASC);


-- -----------------------------------------------------
-- Table `mailboxes_properties`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mailboxes_properties` (
  `mailbox_id` BIGINT UNSIGNED NOT NULL,
  `name` VARCHAR(128) NOT NULL,
  `value` VARCHAR(512) NULL,
  CONSTRAINT `fk_mailboxes_properties_mailbox_id`
    FOREIGN KEY (`mailbox_id`)
    REFERENCES `mailboxes` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;

CREATE INDEX `fk_mailboxes_properties_mailbox_id_idx` ON `mailboxes_properties` (`mailbox_id` ASC);


-- -----------------------------------------------------
-- Table `folders_properties`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `folders_properties` (
  `folder_id` BIGINT UNSIGNED NOT NULL,
  `name` VARCHAR(256) NOT NULL,
  `value` VARCHAR(512) NULL,
  CONSTRAINT `fk_folders_properties_folder_id`
    FOREIGN KEY (`folder_id`)
    REFERENCES `folders` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;

CREATE INDEX `fk_folders_properties_folder_id_idx` ON `folders_properties` (`folder_id` ASC);


-- -----------------------------------------------------
-- Table `servers`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `servers` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `company_id` INT NOT NULL,
  `replica_id` INT NOT NULL DEFAULT 1,
  `change_number` INT NOT NULL DEFAULT 1,
  PRIMARY KEY (`id`),
  CONSTRAINT `fk_servers_company_id`
    FOREIGN KEY (`company_id`)
    REFERENCES `company` (`id`)
    ON DELETE CASCADE
    ON UPDATE CASCADE)
ENGINE = InnoDB;

CREATE INDEX `fk_servers_company_id_idx` ON `servers` (`company_id` ASC);


-- -----------------------------------------------------
-- Table `provisioning_folders`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `provisioning_folders` (
  `locale` VARCHAR(15) NOT NULL,
  `mailbox` VARCHAR(128) NOT NULL DEFAULT "OpenChange Mailbox: %s",
  `deferred_action` VARCHAR(128) NOT NULL DEFAULT "Deferred Action",
  `spooler_queue` VARCHAR(128) NOT NULL DEFAULT "Spooler Queue",
  `common_views` VARCHAR(128) NOT NULL DEFAULT "Common Views",
  `schedule` VARCHAR(128) NOT NULL DEFAULT "Schedule",
  `finder` VARCHAR(128) NOT NULL DEFAULT "Finder",
  `views` VARCHAR(128) NOT NULL DEFAULT "Views",
  `shortcuts` VARCHAR(128) NOT NULL DEFAULT "Shortcuts",
  `reminders` VARCHAR(128) NOT NULL DEFAULT "Reminders",
  `todo` VARCHAR(128) NOT NULL DEFAULT "To-Do",
  `tracked_mail_processing` VARCHAR(128) NOT NULL DEFAULT "Tracked Mail Processing",
  `top_info_store` VARCHAR(128) NOT NULL DEFAULT "Top of Information Store",
  `inbox` VARCHAR(128) NOT NULL DEFAULT "Inbox",
  `outbox` VARCHAR(128) NOT NULL DEFAULT "Outbox",
  `sent_items` VARCHAR(128) NOT NULL DEFAULT "Sent Items",
  `deleted_items` VARCHAR(128) NOT NULL DEFAULT "Deleted Items",
  PRIMARY KEY (`locale`)
) ENGINE = InnoDB;

INSERT INTO `provisioning_folders` SET locale = 'en';

-- -----------------------------------------------------
-- Table `provisioning_special_folders`
-- -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `provisioning_special_folders` (
  `locale` VARCHAR(15) NOT NULL,
  `drafts` VARCHAR(128) NOT NULL DEFAULT "Drafts",
  `calendar` VARCHAR(128) NOT NULL DEFAULT "Calendar",
  `contacts` VARCHAR(128) NOT NULL DEFAULT "Contacts",
  `tasks` VARCHAR(128) NOT NULL DEFAULT "Tasks",
  `notes` VARCHAR(128) NOT NULL DEFAULT "Notes",
  `journal` VARCHAR(128) NOT NULL DEFAULT "Journal",
  PRIMARY KEY (`locale`)
) ENGINE = InnoDB;

INSERT INTO `provisioning_special_folders` SET locale = 'en';
